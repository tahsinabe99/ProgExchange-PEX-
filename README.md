1. Describe how your exchange works.
Exchange firsts reads the product file and makes item object. The we add signal handlers- 1 for SIGUSR1(to know when trader sends message) and one for SIGCHLD(to know when trader disconnects)

The we make named pipes, create child process and run exec to run trader. We also make trader objects and connect to trader pipe. Then exchange sends message and waits for a trader to send signal.
When trader sends message, we parse the command and process it which checks if message is invalid or not. If its a buy sell message we store the order in orderbook and the item's personal order book  or if its amend we amend and store it a tthe very end. If it is cancelled it is added at very end. We match order by using highes buy price and lowest sell price and send approproate message to traders. If SIGCHLD is received we close that tat trader and if all trader is closed exchange closses and frees all dynamically allocated memory
2. Describe your design decisions for the trader and how it's fault-tolerant.
when pipe is unable to open we check that. If there is problem with signal handler we check that.

3. Describe your tests and how to run them.
No tests has been done. For error handling checks has been placed in multiple places
# ProgExchange-PEX-
