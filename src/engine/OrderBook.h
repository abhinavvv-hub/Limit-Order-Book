#pragma once

#include <array>
#include <cstdint>
#include <cstdio>

#include "engine/Types.h"
#include "memory/ObjectPool.h"

/*
=============================================================================
Limit Order Book matching engine.
Design highlights for HFT:
 - All data structures are pre‑allocated (zero dynamic memory on the hot path).
 - Price levels are stored in a fixed‑size array indexed by price.
 - Orders at the same price are kept in an intrusive doubly‑linked list
   using pool indices, giving O(1) append, remove, and top‑of‑book access.
 - A direct id_to_index[] array maps orderId → pool index for O(1) cancel.
 - Branch hints ([[likely]]/[[unlikely]]) guide the CPU’s branch predictor.
=============================================================================
*/

class OrderBook {
public:
    using Price = int64_t;
    static constexpr Price MAX_PRICE = 10000;    /* price range 0 .. MAX_PRICE-1 */
    static constexpr int   MAX_ORDERS = 1024;    /* matching pool size */

    /* Head/tail of the intrusive list for one price level. */
    struct Level {
        int head = -1;
        int tail = -1;
    };

    OrderBook(ObjectPool<Order, MAX_ORDERS>& pool_ref, std::array<int, MAX_ORDERS>& id_map)
        : pool(pool_ref), id_to_index(id_map) {
        id_to_index.fill(-1);    /* -1 means "not in book" */
    }

    /* Process one incoming request */
    void process(const OrderRequest& req) {
        switch (req.msgType) {
            case 'A':
                addOrder(req.orderId, static_cast<int64_t>(req.price),
                         req.quantity, (req.side == 'B') ? Side::Bid : Side::Ask);
                break;
            case 'C':
                cancelOrder(req.orderId);
                break;
            /* 'M' (modify) would be cancel + add, left as exercise. */
            default:
                break;
        }
    }

private:
    // ============================ ADD ================================
    void addOrder(uint64_t orderId, Price price, uint64_t qty, Side side) {
        if (qty == 0) return;

        /* 1. Check cross with the opposite side's best price. */
        if (side == Side::Bid) {
            if (best_ask != MAX_PRICE && price >= best_ask) [[unlikely]] {
                match(price, qty, Side::Bid);
                if (qty == 0) return;
            }
        } 
        else { 
            if (best_bid != -1 && price <= best_bid) [[unlikely]] {
                match(price, qty, Side::Ask);
                if (qty == 0) return; 
            }
        }

        /* 2. Insert remaining quantity into its own side. */
        insertOrder(orderId, price, qty, side);
    }

    // ---------- MATCHING (aggressive order crosses the spread) ---------------
    void match(Price& price, uint64_t& qty, Side aggressiveSide) {
        /* Determine which side's book we will walk. */
        auto& levels = (aggressiveSide == Side::Bid) ? asks : bids;
        Price& best_price = (aggressiveSide == Side::Bid) ? best_ask : best_bid;

        while (qty > 0 && best_price != (aggressiveSide == Side::Bid ? MAX_PRICE : -1)) {
            /* Walk the queue at this price level. */
            Level& level = levels[best_price];
            int idx = level.head;

            while (qty > 0 && idx != -1) {
                Order& resting = pool[idx];

                if (qty >= resting.quantity) {
                    // Full fill – consume this order.
                    qty -= resting.quantity;
                    int nextIdx = resting.next;
                    removeOrderFromLevel(level, idx);
                    deallocateOrder(idx);
                    idx = nextIdx;
                } 
                else {
                    // Partial fill – deduct and we are done.
                    resting.quantity -= qty;
                    qty = 0;
                    break;   // aggressive order fully filled
                }
            }

            // If the price level became empty, find next best price.
            if (level.head == -1) {
                best_price = findNextBest(aggressiveSide == Side::Bid ? Side::Ask : Side::Bid,
                                          best_price);
            }
        }
    }

    // ---------- INSERT into the passive side (after crossing logic) ----------
    void insertOrder(uint64_t orderId, Price price, uint64_t qty, Side side) {
        int poolIdx = pool.allocate();
        if (poolIdx == -1) [[unlikely]] return;   // pool full (should never happen)

        Order& ord    = pool[poolIdx];
        ord.price     = price;
        ord.quantity  = qty;
        ord.side      = side;
        ord.orderId   = orderId;
        ord.next      = -1;
        ord.prev      = -1;

        // Map orderId → pool index for O(1) cancel.
        id_to_index[orderId] = poolIdx;

        auto& levels = (side == Side::Bid) ? bids : asks;
        Level& level = levels[price];

        // Append to the tail of the doubly-linked list at this price level.
        if (level.tail == -1) {
            level.head = level.tail = poolIdx;
        } 
        else {
            pool[level.tail].next = poolIdx;
            ord.prev = level.tail;
            level.tail = poolIdx;
        }

        // Update best price if this is a better price than before.
        if (side == Side::Bid) {
            if (best_bid == -1 || price > best_bid)
                best_bid = price;
        } else {
            if (best_ask == MAX_PRICE || price < best_ask)
                best_ask = price;
        }
    }

    // ======================= CANCEL ==============================
    void cancelOrder(uint64_t orderId) {
        int poolIdx = id_to_index[orderId];
        if (poolIdx == -1) [[unlikely]] return;   // unknown ID

        Order& ord = pool[poolIdx];
        Price price = ord.price;
        Side side   = ord.side;

        auto& levels = (side == Side::Bid) ? bids : asks;
        Level& level = levels[price];

        removeOrderFromLevel(level, poolIdx);
        deallocateOrder(poolIdx);

        // If we removed the last order at a price level that was the best price,
        // scan to find the new best price.
        if (level.head == -1) {
            Price& best = (side == Side::Bid) ? best_bid : best_ask;
            if (price == best) {
                best = findNextBest(side, price);
            }
        }
    }

    // ---------- Helper: remove an order (given its pool index) from a price level ----------
    void removeOrderFromLevel(Level& level, int poolIdx) {
        Order& ord = pool[poolIdx];
        int prev = ord.prev;
        int next = ord.next;

        if (prev != -1) pool[prev].next = next;
        else           level.head = next;

        if (next != -1) pool[next].prev = prev;
        else           level.tail = prev;
    }

    // ---------- Helper: deallocate order and clear its ID mapping ----------
    void deallocateOrder(int poolIdx) {
        id_to_index[pool[poolIdx].orderId] = -1;
        pool.deallocate(poolIdx);
    }

    // ---------- Find the next best price when the current best level becomes empty ----------
    [[nodiscard]] Price findNextBest(Side side, Price current) const {
        if (side == Side::Bid) {
            // Scan downwards (lower prices)
            for (Price p = current - 1; p >= 0; --p) {
                if (bids[p].head != -1) return p;
            }
            return -1;   // no bids left
        } else {
            // Scan upwards (higher prices)
            for (Price p = current + 1; p < MAX_PRICE; ++p) {
                if (asks[p].head != -1) return p;
            }
            return MAX_PRICE;   // no asks left
        }
    }

    // ========== MEMBERS ======================================================
    ObjectPool<Order, MAX_ORDERS>& pool;
    std::array<int, MAX_ORDERS>&   id_to_index;

    // Price level heads. Indices correspond directly to price.
    std::array<Level, MAX_PRICE> bids;
    std::array<Level, MAX_PRICE> asks;

    // Cached best prices to avoid scanning for top‑of‑book on every query.
    Price best_bid = -1;
    Price best_ask = MAX_PRICE;
};
