#pragma once

#include <cstdint>

/* Core order and network message types */
enum class Side : uint8_t {
    Bid = 0,
    Ask = 1
};

/* Intrusive doubly-linked list node for an order inside the order book. */
/* 'next' and 'prev' are indices into the pre-allocated order pool, NOT pointers. */
/* This keeps the pool compact and cache-friendly, and avoids any dynamic memory. */
struct Order {
    int64_t  price;    /* price in ticks */
    uint64_t quantity; /* remaining quantity */
    Side     side;
    uint64_t orderId;  /* client-assigned ID, must be < MAX_ORDERS */
    int32_t  next;     /* pool index of next order at same price level (-1 if none) */
    int32_t  prev;     /* pool index of previous order (-1 if none) */
};

/* Compact network message. Packed to avoid padding across different systems. */
#pragma pack(push, 1)
struct OrderRequest {
    char     msgType;   /* 'A' for new order, 'C' for Cancel */
    uint64_t orderId;
    int64_t  price;     /* price in ticks */
    uint64_t quantity;  /* order quantity */
    char     side;      /* 'B' or 'A' */
};
#pragma pack(pop)
