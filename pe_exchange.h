#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include "pe_common.h"

#define LOG_PREFIX "[PEX]"
#define MAX_TRADERS 10
#define MAX_ITEMS 102
#define MAX_ORDERS 600


typedef struct trader_item trader_item;
typedef struct order order;
typedef struct trader trader;
typedef struct items items;

struct trader_item{
    char *name;
    int quantity;
    long long int cash;
};

struct trader{
    int trader_id;
    trader_item *arr[MAX_ITEMS];
    int cancel_no;
    int cancel_arr[MAX_ITEMS];
};

struct items{
    char *product_name;
    int buy_levels;
    int sell_levels;
    order *buy_orders[MAX_ORDERS];
    order *sell_orders[MAX_ORDERS];

};

struct order{
    char *product_name;
    int buy_sell; //0 is buy 1 is sell
    int quantity;
    int price;
    int trader_id;
    int order_id;
    int no_of_orders;
    int item_no;
};




#endif
