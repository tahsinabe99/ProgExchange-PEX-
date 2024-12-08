Introducing ProgExchange (PEX), a cutting-edge platform for computer programmers to buy and sell
high-demand components in a virtual marketplace. The need for in-person trading has been replaced
by a state-of-the-art digital marketplace, allowing for greater accessibility and faster transactions,
while providing a safe and convenient way for seasoned programmers to connect and make transactions.  

There are two key components of ProgExchange:
the exchange itself, which will handle all incoming orders, and an auto-trading program that executes
trades based on predefined conditions.

Exchange
The purpose of the ProgExchange is to allow trading to happen in an efficient and orderly manner. It
receives orders from traders and matches them, allowing the buying and selling of computer components.
The exchange also tracks the cash balance of each trader (which starts at $0 for each trading session).
For providing such trading avenue, ProgExchange collects a 1% transaction fee on all successful
orders.

Trader
Traders carry out their buying and selling activities by placing orders on the exchange.

Auto-trader
The program also has an auto-trader. Auto-trader is very simple: as soon as a SELL order is available in the exchange, it
will place an opposite BUY order to attempt to buy the item.

The exchange should attempt to match orders once there is at least one BUY and one SELL order for
any particular product. Orders match if the price of the BUY order is larger than or equal to the price
of the SELL order. The matching price is the price of the older order. Of the two matched traders, the
exchange charges 1% transaction fee to the trader who placed the order last, rounded to the nearest
dollar (eg: $4.50 -> $5.00).

How to run the program
./pe_exchange [product file] [trader 0] [trader 1] ... [trader n] where n is the number of traders

The following example uses a product file named products.txt and trader binaries trader_a and trader_b:
./pe_exchange products.txt ./trader_a ./trader_b

