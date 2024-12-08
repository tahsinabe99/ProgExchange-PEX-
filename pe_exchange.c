/**
 * comp2017 - assignment 3
 * Tahsinul Abedin
 * tabe5206
 */

#include "pe_exchange.h"
int exchange_fds[MAX_TRADERS];
int trader_fds[MAX_TRADERS];
char products[MAX_ITEMS][17];
int products_number=0;
pid_t all_trader_pid[MAX_TRADERS]={0};
long long int exchange_fees=0;
char trader_message[128];



order *all_orders[MAX_ORDERS];
int all_order_index=0;

trader *all_trader[MAX_TRADERS];

items *all_items[MAX_ITEMS];

order *garbage_order[MAX_ORDERS];
int garbage_index=0;

//function to handle exchange teardown
void exchange_close(){
	int not_close=0;
	//to check if all traders are disconnected
	for(int i=0; i<MAX_TRADERS; i++){
		if(all_trader_pid[i]>0){
			not_close=1;
		}
	}
	//all traders are disconnected so
	if(not_close==0){
		//freeing all dynamically allocated memory
		for(int j=0; j<all_order_index; j++){
			if(all_orders[j]!=NULL){
				free(all_orders[j]);
				all_orders[j]=NULL;
			}
			
		}		

		for(int z=0; z<products_number;z++){
			if(all_items[z]!=NULL){
				free(all_items[z]);
				all_items[z]=NULL;
			}
		}

		for(int x=0; x<MAX_TRADERS; x++){
			if(all_trader[x]!=NULL){
				for(int k=0; k<products_number; k++){
					if(all_trader[x]!=NULL){
						free(all_trader[x]->arr[k]);
						all_trader[x]->arr[k]=NULL;
					}
									}
				free(all_trader[x]);
				all_trader[x]=NULL;
			}
		}
		printf("%s Trading completed\n", LOG_PREFIX);
		printf("%s Exchange fees collected: $%lld\n", LOG_PREFIX, exchange_fees);	
		exit(0);
	}
}

//function to handle disconnection of trader
//might have to free traders
void trader_disconnect(pid_t p_id){
	int i;
	for(i=0; i<MAX_TRADERS; i++){
		if(p_id==all_trader_pid[i]){
			break;
		}		
	}
	all_trader_pid[i]=0;
	close(exchange_fds[i]);
	close(trader_fds[i]);
	exchange_fds[i]=-1;
	trader_fds[i]=-1;
	char exc_pipe[50];
	char trd_pipe[50];
	sprintf(exc_pipe, FIFO_EXCHANGE, i);
	sprintf(trd_pipe, FIFO_TRADER, i);
	unlink(exc_pipe);
	unlink(trd_pipe);
	printf("%s Trader %d disconnected\n", LOG_PREFIX, i);
	exchange_close();
}

//to check if the product order received exists in the products book;
//does not work atm
int check_product_validity(char *item){
	for(int i=0; i<products_number; i++){
		if(strcmp(products[i], item)==0){
			return i;
		}
	}
	return -1;

}

//we do not use it for cancel order
int check_orderid_valid(int index, int order_id){
	//checking if order id already exists
	//int incremental_flag=-1;
	trader *trader_to_check=all_trader[index];
	int highest_order_id=-1;
	for(int i=0;i<all_order_index; i++){
		//here we check order id is already there
		if(all_orders[i]->trader_id==index){
			if(all_orders[i]->order_id==order_id){
				return -1;
			}
			highest_order_id=all_orders[i]->order_id;
		}
	}

	//checking if order id exists in complete/cancel orderid
	for(int i=0; i<trader_to_check->cancel_no; i++){
		if(order_id==trader_to_check->cancel_arr[i]){
			return -1;
		}
	}

	//checking if order id is incremental and in order
	if(order_id== (highest_order_id+1) ){
		return order_id;
	}
	//to deal with cancel order ids
	//once order is cancelled it no longer exists in order book
	//so if order 0 is cancelled, putting order id 1 will show as not valid
	//so we  also gotta check if order id is incremental from cancelled order
	else{
		int highest_cancel_orderid=-1;
		
		for(int i=0; i<trader_to_check->cancel_no; i++){
			if(highest_cancel_orderid< trader_to_check->cancel_arr[i]){
				highest_cancel_orderid=trader_to_check->cancel_arr[i];
			}
		}
		if(order_id ==(highest_cancel_orderid+1)){
			return order_id;
		}
	}
	

	return -1;
}

void inform_other_traders(char *product, int quantity, int price, int buy_sell, int cancel, int amend, int index){
	char send_message[128];
	if(buy_sell==1 && cancel==0 && amend==0){
		sprintf(send_message, "MARKET SELL %s %d %d;",product, quantity,price);
	}
	else if(buy_sell==0 && cancel==0 && amend==0){
		sprintf(send_message, "MARKET BUY %s %d %d;",product, quantity,price);
	}
	else if(cancel==1 && amend==0 && buy_sell==0){
		sprintf(send_message, "MARKET BUY %s 0 0;", product);
	}
	else if(cancel==1 && amend==0 && buy_sell==1){
		sprintf(send_message, "MARKET SELL %s 0 0;", product);
	}
	else if(amend==1 && cancel==0 && buy_sell==0){
		sprintf(send_message, "MARKET BUY %s %d %d;",product, quantity,price);

	}
	else if(amend==1 && cancel==0 && buy_sell==1){
		sprintf(send_message, "MARKET SELL %s %d %d;",product, quantity,price);
	}
	for(int i=0; i<MAX_TRADERS; i++){
		if((all_trader_pid[i]!=0)  && (i!=index)){
			write(exchange_fds[i], &send_message, strlen(send_message));
			kill(all_trader_pid[i], SIGUSR1);
		}
	}	
}

int check_message_validity_buy_sell(int index){
	char *token=strtok(trader_message, " ");
	int buy_sell=-1;
	if(strcmp(token,"BUY")==0){
		buy_sell=0; //indicating buy
	}
	else if(strcmp(token,"SELL")==0){
		buy_sell=1; //indicating sell
	}
	if(token!=NULL){
		token=strtok(NULL, " ");//order id  
		char *order_id=token;
		token=strtok(NULL, " ");//product
		char *product=token;
		token=strtok(NULL, " ");//quantity
		char* quantity=token;
		token=strtok(NULL, " ");//price
		char *price=token;
		//checking if message exists beyond what is necessary
		token=strtok(NULL, " ");
		//price=strtok(price, ";");
		
		

		if(order_id==NULL || product==NULL||quantity==NULL || price==NULL || token!=NULL){
			return -1;
		}
		else {

			int check= check_product_validity(product);
			int quantity_no=atoi(quantity); 
			int price_no=atoi(price);
			int order_id_no=atoi(order_id);
			order_id_no=check_orderid_valid(index,order_id_no);
			
			if(quantity_no <1 || quantity_no> 999999  || price_no<1 || price_no>999999 || order_id_no<0 || order_id_no>999999 || check<0){
				return -1;
			}
			else{
				order *new_order=malloc(sizeof(order));
				new_order->product_name=product;
				new_order->buy_sell=buy_sell;
				new_order->price=price_no;
				new_order->quantity=quantity_no;
				new_order->trader_id=index;
				new_order->order_id=order_id_no;
				new_order->no_of_orders=1;
				new_order->item_no=check;
				all_orders[all_order_index]=new_order;
				all_order_index++;
				if(buy_sell==0){
					all_items[check]->buy_orders[all_items[check]->buy_levels]=new_order;
					all_items[check]->buy_levels++;					
				}
				else if(buy_sell==1){
					all_items[check]->sell_orders[all_items[check]->sell_levels]=new_order;
					all_items[check]->sell_levels++;
				}
				inform_other_traders(product, quantity_no, price_no, buy_sell,0,0, index);

				return order_id_no;
			}
		}
	}
	return -1;
}

int check_orderid_exists(int index, int order_id){
	//checking if order id exists
	for(int i=0;i<all_order_index; i++){
		if(all_orders[i]->trader_id==index){
			if(all_orders[i]->order_id==order_id){
				return order_id;
			}
		}
	}

	return -1;
}

int check_message_validity_ammend(int index){
	char *token=strtok(trader_message, " ");
	char *order_id=NULL;
	char *quantity=NULL;
	char *price=NULL;
	if(token!=NULL){
		token=strtok(NULL, " ");
		order_id=token;  
		token=strtok(NULL, " ");
		quantity=token;
		token=strtok(NULL, " ");
		price=token;
		//checking is there more to message beyond what is needed
		token=strtok(NULL, " ");
		price=strtok(price, ";");
		
		//check if amend message is incomplete
		if(order_id==NULL || quantity==NULL ||price==NULL ||token!=NULL){
			return -1;
		}
		else{
			int order_id_no=atoi(order_id);
			order_id_no=check_orderid_exists(index, order_id_no);
			int quantity_no=atoi(quantity);
			int price_no=atoi(price);
			//check if values match requirements
			if(order_id_no<0 || order_id_no>999999 || price_no<1 || price_no>999999 || quantity_no<1 || quantity_no>999999 ){
				return -1;
			}
			else{
				//find the order to be modified
				order *modified_order=NULL;
				int order_index=-1;
				for(int i=0; i<all_order_index; i++){
					if(all_orders[i]->order_id==order_id_no && all_orders[i]->trader_id==index){
						modified_order=all_orders[i];
						order_index=i;
						break;
					}
				}
				if(modified_order!=NULL){
					//modifying the order.
					if(quantity!=NULL && price !=NULL){
						modified_order->quantity=quantity_no;
						modified_order->price=price_no;
					}
					else{
						exit(1);
					}
					
					

					//loose time priority

					//add the order to last and change existing order to null
					all_orders[order_index]=NULL;
					
					//put all null orders to last
					int j=0;
					for(int i=0; i<all_order_index; i++){
						if(all_orders[i]!=NULL){
							all_orders[j]= all_orders[i];
							j++; 
						}
					}
					while(j<MAX_ORDERS){
						all_orders[j]=NULL;
						j++;
					}
					all_orders[all_order_index-1]=modified_order;
					

					//loose time priority in item's order list
					if(modified_order->buy_sell==0){
					 	//first we find that order and put it to null
						int lvl= all_items[modified_order->item_no]->buy_levels;
						for(int i=0; i<lvl; i++){
							if(all_items[modified_order->item_no]->buy_orders[i]->order_id==modified_order->order_id && all_items[modified_order->item_no]->buy_orders[i]->trader_id==modified_order->trader_id){
								all_items[modified_order->item_no]->buy_orders[i]=NULL;
								break;	
							}
						}

						//put null orders at last 
						j=0;
						int item_index=modified_order->item_no;
						for(int i=0; i<all_items[item_index]->buy_levels; i++){
							if(all_items[item_index]->buy_orders[i]!=NULL){
								all_items[item_index]->buy_orders[j]= all_items[item_index]->buy_orders[i];
								j++; 
							}
						}
						while(j<MAX_ORDERS){
							all_items[item_index]->buy_orders[j]=NULL;
							j++;
						}
						//set the modified order at last
						all_items[modified_order->item_no]->buy_orders[lvl-1]=modified_order;
						

					}
					else{
						//first we find that order and put it to null
						int lvl= all_items[modified_order->item_no]->sell_levels;
						for(int i=0; i<lvl; i++){
							if(all_items[modified_order->item_no]->sell_orders[i]->order_id==modified_order->order_id && all_items[modified_order->item_no]->sell_orders[i]->trader_id==modified_order->trader_id){
								all_items[modified_order->item_no]->sell_orders[i]=NULL;
								break;	
							}
						}

						//put null orders at last 
						j=0;
						int item_index=modified_order->item_no;
						for(int i=0; i<all_items[item_index]->sell_levels; i++){
							if(all_items[item_index]->sell_orders[i]!=NULL){
								all_items[item_index]->sell_orders[j]= all_items[item_index]->sell_orders[i];
								j++; 
							}
						}
						while(j<MAX_ORDERS){
							all_items[item_index]->sell_orders[j]=NULL;
							j++;
						}
						//set the modified order at last
						all_items[modified_order->item_no]->sell_orders[lvl-1]=modified_order;
					}


					//informing all traders
					inform_other_traders(all_items[modified_order->item_no]->product_name, quantity_no, price_no, modified_order->buy_sell, 0, 1, index);
					return order_id_no;
				}
			}
		}


	}
	
	return -1;
}

int check_message_validity_cancel(int index){
	char *token=strtok(trader_message, " ");
	char *order_id=NULL;
	if(token!=NULL){
		token=strtok(NULL, " ");
		order_id=token;
	}
	if(order_id==NULL){
		return -1;
	}
	else{
		int order_id_no=atoi(order_id);
		//check if order exists
		int check=check_orderid_exists(index, order_id_no);
		if(check==-1){
			return -1;
		}
		//removing order from orderbook
		order * removed_order=NULL;
		for(int i=0; i<all_order_index; i++){
			if(all_orders[i]->order_id==order_id_no && all_orders[i]->trader_id==index){
				removed_order=all_orders[i];
				all_orders[i]=NULL;
				break;
			}
		}
		//pushing null orders at the end.
		int j=0;
		for(int i=0; i<all_order_index; i++){
			if(all_orders[i]!=NULL){
				all_orders[j]= all_orders[i];
				j++; 
			}
		}
		while(j<MAX_ORDERS){
			all_orders[j]=NULL;
			j++;
		}
		//ensuring we decrease all_order_index
		all_order_index--;
		//remove from item buy or item sell order list
		items *item_with_order=NULL;
		int order_index=-1;
		for(int i=0; i<products_number; i++){
			if(removed_order->buy_sell==0){
				//order that needs to be cancelled is buy order
				for(int x=0; x< all_items[i]->buy_levels; x++){
					if(all_items[i]->buy_orders[x]->order_id ==order_id_no && all_items[i]->buy_orders[x]->trader_id ==index ){
						item_with_order=all_items[i];
						order_index=x;
						break;
					}
				}

				if(item_with_order!=NULL && order_index>-1){
					break;
				}
			}
			//order that needs to be cancelled is sell order
			else{
				for(int x=0; x< all_items[i]->sell_levels; x++){
					if(all_items[i]->sell_orders[x]->order_id ==order_id_no && all_items[i]->sell_orders[x]->trader_id ==index ){
						item_with_order=all_items[i];
						order_index=x;
						break;
					}
				}
				if(item_with_order!=NULL && order_index>-1){
					break;
				}
			}
		}

		//removing order from item 
		if(removed_order->buy_sell==0){
			//from buy orders
			item_with_order->buy_orders[order_index]=NULL;
			//putting null order at the last
			int j=0;
			for(int i=0; i<item_with_order->buy_levels; i++){
				if(item_with_order->buy_orders[i]!=NULL){
					item_with_order->buy_orders[j]= item_with_order->buy_orders[i];
					j++; 
				}	
			}
			while(j<MAX_ORDERS){
				item_with_order->buy_orders[j]=NULL;
				j++;
			}
			//decreasing buy levels
			item_with_order->buy_levels--;
			}
			else{
				//from sell orders
				item_with_order->sell_orders[order_index]=NULL;
				//puttomg null order at the last
				int j=0;
				for(int i=0; i<item_with_order->sell_levels; i++){
					if(item_with_order->sell_orders[i]!=NULL){
						item_with_order->sell_orders[j]= item_with_order->sell_orders[i];
						j++; 
					}	
				}
				while(j<MAX_ORDERS){
					item_with_order->sell_orders[j]=NULL;
					j++;
				}
				//decreasing sell levels
				item_with_order->sell_levels--;

			}
		if(removed_order==NULL){
			return -1;
		}
		else{
			all_trader[index]->cancel_arr[all_trader[index]->cancel_no]=removed_order->order_id;
			all_trader[index]->cancel_no++;
			inform_other_traders(item_with_order->product_name, removed_order->quantity, removed_order->price, removed_order->buy_sell,1,0,index);
			free(removed_order);
			removed_order=NULL;
			return order_id_no;
		}

	}	

	
	return -1;
}



void print_trader_position(){
	printf("%s	--POSITIONS--\n", LOG_PREFIX);
	for(int i=0; i<MAX_TRADERS; i++){
		if(all_trader[i]!=NULL){
			printf("%s	Trader %d: ", LOG_PREFIX, all_trader[i]->trader_id);
			for(int j=0; j<products_number; j++){
				printf("%s %d ($%lld)", all_trader[i]->arr[j]->name, all_trader[i]->arr[j]->quantity, all_trader[i]->arr[j]->cash);

				if(j<(products_number-1)){
					printf(", ");
				}else{
					printf("\n");
				}
			}			
		}
	}
}



int compare(const void *order1, const void *order2){
	order **orderdup1= (order **) order1;
	order **orderdup2= (order **) order2;
	if(orderdup1!=NULL && orderdup2!=NULL){
		return ((*orderdup2)->price - (*orderdup1)->price);
	}
	else{
		printf("WE HAVE CAUGHT A NULL");
		exit(1);
	}
	
	return (-99999);
	
}


void free_garbage_order(){
	for(int i=0; i<garbage_index;i++){
		if(garbage_order[i]!=NULL){
			free(garbage_order[i]);
			garbage_order[i]=NULL;
		}
		
	}
	garbage_index=0;
}

items* duplicating_item(items * given_item){
	items* duplicate_item=malloc(sizeof(items));
	duplicate_item->product_name=(given_item->product_name); 
	duplicate_item->buy_levels=given_item->buy_levels;
	duplicate_item->sell_levels=given_item->sell_levels;

	for(int i=0;i<MAX_ORDERS; i++){
		if(given_item->buy_orders[i]!=NULL){
			duplicate_item->buy_orders[i]=malloc(sizeof(order));
			memcpy(duplicate_item->buy_orders[i], given_item->buy_orders[i], sizeof(order));
			garbage_order[garbage_index]=duplicate_item->buy_orders[i];
			garbage_index++;
		}
		else{
			duplicate_item->buy_orders[i]=NULL;
		}
	}

	for(int i=0;i<MAX_ORDERS; i++){
		if(given_item->sell_orders[i]!=NULL){
			duplicate_item->sell_orders[i]=malloc(sizeof(order));
			memcpy(duplicate_item->sell_orders[i], given_item->sell_orders[i], sizeof(order));
			garbage_order[garbage_index]=duplicate_item->sell_orders[i];
			garbage_index++;
		}
		else{
			duplicate_item->sell_orders[i]=NULL;
		}
	}
	return duplicate_item;
}



//function will return duplicate item suited to be used for order book
items * handle_duplicate_order1(items* given_item){
	//we duplicate the given item struct so we do not modify the original
	items *duplicate_item=duplicating_item(given_item);


	if(duplicate_item!=NULL){

		//for buy orders
		if(duplicate_item->buy_levels>1){
			//sorting in descending order 
			qsort(duplicate_item->buy_orders, duplicate_item->buy_levels, sizeof(order*), compare);

			
			for(int i=0; i<given_item->buy_levels; i++){
				for(int j=(i+1); j<given_item->buy_levels; j++){
					if(duplicate_item->buy_orders[i]!=NULL  && duplicate_item->buy_orders[j]!=NULL){
						if( ((duplicate_item->buy_orders[i]->price)==( duplicate_item->buy_orders[j]->price) ) && (i!=j) ){
							duplicate_item->buy_orders[i]->quantity= (duplicate_item->buy_orders[i]->quantity ) + (duplicate_item->buy_orders[j]->quantity);
							duplicate_item->buy_orders[j]=NULL;
							duplicate_item->buy_levels=duplicate_item->buy_levels-1;
							duplicate_item->buy_orders[i]->no_of_orders= duplicate_item->buy_orders[i]->no_of_orders+1;
						}
					}
					
				}
			}
			//removing null from array
			int j=0;
			for(int i=0; i<given_item->buy_levels; i++){
				if(duplicate_item->buy_orders[i]!=NULL){
					duplicate_item->buy_orders[j]= duplicate_item->buy_orders[i];
					j++;
				}
			}
			while(j<MAX_ORDERS){	
				duplicate_item->buy_orders[j]=NULL;
				j++;
			}
		}

		//for buy orders
		if(duplicate_item->sell_levels>1){
			qsort(duplicate_item->sell_orders, duplicate_item->sell_levels, sizeof(order*), compare);

			for(int i=0; i<given_item->sell_levels; i++){
				for(int j=(i+1); j<given_item->sell_levels; j++){
					if(duplicate_item->sell_orders[i]!=NULL  && duplicate_item->sell_orders[j]!=NULL){
						if( ((duplicate_item->sell_orders[i]->price)==( duplicate_item->sell_orders[j]->price) ) && (i!=j) ){
							duplicate_item->sell_orders[i]->quantity= (duplicate_item->sell_orders[i]->quantity ) + (duplicate_item->sell_orders[j]->quantity);
							duplicate_item->sell_orders[j]=NULL;
							duplicate_item->sell_levels=duplicate_item->sell_levels-1;
							duplicate_item->sell_orders[i]->no_of_orders= duplicate_item->sell_orders[i]->no_of_orders+1;
						}
					}
					
				}
			}

			int j=0;
			for(int i=0; i<given_item->sell_levels; i++){
				if(duplicate_item->sell_orders[i]!=NULL){
					duplicate_item->sell_orders[j]= duplicate_item->sell_orders[i];
					j++;
				}
			}
			while(j<MAX_ORDERS){	
				duplicate_item->sell_orders[j]=NULL;
				j++;
			}

		}
	
	}

	return duplicate_item;
}

//this deals with match orders where buying quantity is smaller than selling quantity
void matchorder_backend_buy_smaller(order * highest_buy_order, int highest_buy_order_index, order *lowest_sell_order, int lowest_sell_order_index, int item_index, int item_buy_order_index){
	//handling trader item quantity
	int buy_traader_id=highest_buy_order->trader_id;
	int sell_trader_id=lowest_sell_order->trader_id;
	all_trader[buy_traader_id]->arr[item_index]->quantity+=highest_buy_order->quantity;
	all_trader[sell_trader_id]->arr[item_index]->quantity-=highest_buy_order->quantity;

	
	//decreasing sell  order quantity
	lowest_sell_order->quantity= lowest_sell_order->quantity - highest_buy_order->quantity;
	
	

	//removing order from item buy list as quantity is adequate and order fulfilled
	all_items[item_index]->buy_orders[item_buy_order_index]=NULL;
	//putting NULL items at the last
	int j=0;
	for(int i=0; i<all_items[item_index]->buy_levels; i++){
		if(all_items[item_index]->buy_orders[i]!=NULL){
			all_items[item_index]->buy_orders[j]= all_items[item_index]->buy_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_items[item_index]->buy_orders[j]=NULL;
		j++;
	}

	//decreasing buy levels
	all_items[item_index]->buy_levels--;

	
	//removing order from order list
	order *removed_order=all_orders[highest_buy_order_index];
	all_orders[highest_buy_order_index]=NULL;
	//pushing null orders at the end.
	j=0;
	for(int i=0; i<all_order_index; i++){
		if(all_orders[i]!=NULL){
			all_orders[j]= all_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_orders[j]=NULL;
		j++;
	}
	//ensuring we decrease all_order_index
	all_order_index--;

	if(removed_order!=NULL){
		garbage_order[garbage_index]=removed_order;
		garbage_index++;
	}

}

void match_backend_buy_sell_same(order * highest_buy_order, int highest_buy_order_index, order *lowest_sell_order, int lowest_sell_order_index, int item_index,int item_buy_order_index ,int item_sell_order_index){
	//handling trader item quantity
	int buy_traader_id=highest_buy_order->trader_id;
	int sell_trader_id=lowest_sell_order->trader_id;
	all_trader[buy_traader_id]->arr[item_index]->quantity+=highest_buy_order->quantity;
	all_trader[sell_trader_id]->arr[item_index]->quantity-=highest_buy_order->quantity;

	//removing order from item sell order 
	all_items[item_index]->sell_orders[item_sell_order_index]=NULL;

	//removing order from item buy list
	all_items[item_index]->buy_orders[item_buy_order_index]=NULL;

	//putting null item as the last in buy list
	int j=0;
	for(int i=0; i<all_items[item_index]->buy_levels; i++){
		if(all_items[item_index]->buy_orders[i]!=NULL){
			all_items[item_index]->buy_orders[j]= all_items[item_index]->buy_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_items[item_index]->buy_orders[j]=NULL;
		j++;
	}

	//putting null items at the last in sell list
	j=0;
	for(int i=0; i<all_items[item_index]->sell_levels; i++){
		if(all_items[item_index]->sell_orders[i]!=NULL){
			all_items[item_index]->sell_orders[j]= all_items[item_index]->sell_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_items[item_index]->sell_orders[j]=NULL;
		j++;
	}

	//decreasing buy and sell levels of that item
	all_items[item_index]->sell_levels--;
	all_items[item_index]->buy_levels--;

	//removing buy and sell order from order list 
	//removing order from order list
	order *removed_order_buy=all_orders[highest_buy_order_index];
	order *removed_order_sell=all_orders[lowest_sell_order_index];
	all_orders[highest_buy_order_index]=NULL;
	all_orders[lowest_sell_order_index]=NULL;
	//pushing null orders at the end.
	j=0;
	for(int i=0; i<all_order_index; i++){
		if(all_orders[i]!=NULL){
			all_orders[j]= all_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_orders[j]=NULL;
		j++;
	}
	//ensuring we decrease all_order_index
	all_order_index--;
	all_order_index--;

	if(removed_order_buy!=NULL && removed_order_sell!=NULL){
		garbage_order[garbage_index]=removed_order_buy;
		garbage_index++;
		garbage_order[garbage_index]=removed_order_sell;
		garbage_index++;
	}






}



//this deals with match order where selling quantity is smaller than buying quantity
void matchorder_backend_sell_smaller(order * highest_buy_order, int highest_buy_order_index, order *lowest_sell_order, int lowest_sell_order_index, int item_index, int item_sell_order_index){
	
	//handling trader item quantity
	int buy_traader_id=highest_buy_order->trader_id;
	int sell_trader_id=lowest_sell_order->trader_id;
	all_trader[buy_traader_id]->arr[item_index]->quantity+=lowest_sell_order->quantity;
	all_trader[sell_trader_id]->arr[item_index]->quantity-=lowest_sell_order->quantity;

	//decreasing buy order quantity
	highest_buy_order->quantity = highest_buy_order->quantity- lowest_sell_order->quantity;

	//removing order from item sell list as quantity is adequate and order fulfilled
	all_items[item_index]->sell_orders[item_sell_order_index]=NULL;
	//putting NULL items at last
	int j=0;
	for(int i=0; i<all_items[item_index]->sell_levels; i++){
		if(all_items[item_index]->sell_orders[i]!=NULL){
			all_items[item_index]->sell_orders[j]= all_items[item_index]->sell_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_items[item_index]->sell_orders[j]=NULL;
		j++;
	}
	//decrease sell levels
	all_items[item_index]->sell_levels--;

	//removing sell order from order list
	order *removed_order=all_orders[lowest_sell_order_index];
	all_orders[lowest_sell_order_index]=NULL;

	j=0;
	for(int i=0; i<all_order_index; i++){
		if(all_orders[i]!=NULL){
			all_orders[j]= all_orders[i];
			j++; 
		}
	}
	while(j<MAX_ORDERS){
		all_orders[j]=NULL;
		j++;
	}
	all_order_index--;

	if(removed_order!=NULL){
		garbage_order[garbage_index]=removed_order;
		garbage_index++;
	}

}


void match_order(){

	//for each item
	for(int i=0; i<products_number;i++){
		
		if(all_items[i]->buy_levels>0 && all_items[i]->sell_levels>0){ 
			int fixed_level=all_items[i]->buy_levels;
			for(int e=0; e<fixed_level; e++){

				//no need to carry all this buy or sell level is 0
				if(all_items[i]->buy_levels==0 || all_items[i]->sell_levels==0){
					break;
				}
			

				//---from here is the algorithm to match one order---//
				int j=0;
				int lowest_sell_order_item_index=0;
				order *lowest_sell_order= all_items[i]->sell_orders[0];
				for(j=0; j<all_items[i]->sell_levels; j++){
					if(lowest_sell_order->price > all_items[i]->sell_orders[j]->price){
						lowest_sell_order=all_items[i]->sell_orders[j];
						lowest_sell_order_item_index=j;

					}
				}
				//find highest price buy order in item's array
				int k=0;
				int highest_buy_order_item_index=0;
				
				order *highest_buy_order= all_items[i]->buy_orders[0];
				for(k=0; k<all_items[i]->buy_levels; k++){
					if(highest_buy_order->price < all_items[i]->buy_orders[k]->price){
						highest_buy_order=all_items[i]->buy_orders[k];
						highest_buy_order_item_index=k;
					}
				}

				
				if(highest_buy_order->price>=lowest_sell_order->price){

					//getting the index of the highest buy and lowest sell order in order array so we can edit it later on;
					int highest_buy_index;
					int lowest_sell_index;
					for(int g=0; g<all_order_index; g++){
						if(highest_buy_order== all_orders[g]){
							highest_buy_index=g;
							break;
						}
					}

					for(int g=0; g<all_order_index; g++){
						if(lowest_sell_order== all_orders[g]){
							lowest_sell_index=g;
							break;
						}
					}
					//lomg long int used as signal 2 test case has crazy long numebr.
					long long int cost;
					long long int fee;
					//float float_fee;
					int buy_trader_id=highest_buy_order->trader_id;
					int sell_trader_id=lowest_sell_order->trader_id;
					int quantity;

					//making sure oldest price is used in value calculation
					if(highest_buy_index<lowest_sell_index){
						
						//if the buy quantity is smaller we use the smaller quantity
						if( highest_buy_order->quantity<lowest_sell_order->quantity){
							
							quantity=highest_buy_order->quantity;
							cost=(((long long int)highest_buy_order->quantity)* (long long int)(highest_buy_order->price));
							//float_fee= (cost*0.01);
							fee=round(cost*0.01);
							
							matchorder_backend_buy_smaller(highest_buy_order,highest_buy_index, lowest_sell_order, lowest_sell_index,i,highest_buy_order_item_index);

							//handling cost of trader
							all_trader[buy_trader_id]->arr[i]->cash-=cost;
							all_trader[sell_trader_id]->arr[i]->cash+=cost;
							all_trader[sell_trader_id]->arr[i]->cash-=fee;

						}
						//if the buy quantity is equal to sell quantity 
						else if(highest_buy_order->quantity == lowest_sell_order->quantity){
							cost=((long long int)(highest_buy_order->quantity) *(long long int)(highest_buy_order->price));
							quantity=highest_buy_order->quantity;
							//float_fee= (cost*0.01);
							fee=round(cost*0.01);
							
							match_backend_buy_sell_same(highest_buy_order, highest_buy_index,lowest_sell_order, lowest_sell_index, i, highest_buy_order_item_index,lowest_sell_order_item_index);

							//handling cost of trader
							all_trader[buy_trader_id]->arr[i]->cash-=cost;
							all_trader[sell_trader_id]->arr[i]->cash+=cost;
							all_trader[sell_trader_id]->arr[i]->cash-=fee;

						}
						//if buy quantity is larger than sell quantity
						else{
							cost=((long long int) (lowest_sell_order->quantity) *(long long int)(highest_buy_order->price));
							quantity=lowest_sell_order->quantity;
							//float_fee= (cost*0.01);
							fee=round(cost*0.01);
							
							matchorder_backend_sell_smaller(highest_buy_order,highest_buy_index, lowest_sell_order, lowest_sell_index,i,lowest_sell_order_item_index);
							all_trader[buy_trader_id]->arr[i]->cash-=cost;
							all_trader[sell_trader_id]->arr[i]->cash+=cost;
							all_trader[sell_trader_id]->arr[i]->cash-=fee;

						}
						//front end of matching orders
						printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%lld, fee: $%lld.\n", LOG_PREFIX, highest_buy_order->order_id,
							highest_buy_order->trader_id, lowest_sell_order->order_id, lowest_sell_order->trader_id, cost,fee);
					}
					else{
						//here sell is the older price
						if( highest_buy_order->quantity<lowest_sell_order->quantity){
							cost=(long long int)(highest_buy_order->quantity) *(long long int)(lowest_sell_order->price);
							quantity=highest_buy_order->quantity;
							//float_fee= (cost*0.01);
							fee=round(cost*0.01);
							
							matchorder_backend_buy_smaller(highest_buy_order,highest_buy_index, lowest_sell_order, lowest_sell_index,i,highest_buy_order_item_index);
							//handling trader cost
							all_trader[buy_trader_id]->arr[i]->cash-=cost;
							all_trader[sell_trader_id]->arr[i]->cash+=cost;
							all_trader[buy_trader_id]->arr[i]->cash-=fee;

						}
						else if(highest_buy_order->quantity == lowest_sell_order->quantity){
							cost=((long long int)(highest_buy_order->quantity) * (long long int)(lowest_sell_order->price));
							quantity=highest_buy_order->quantity;
							//float_fee= (cost*0.01);
							fee=round(cost*0.01);
							
							match_backend_buy_sell_same(highest_buy_order, highest_buy_index,lowest_sell_order, lowest_sell_index, i, highest_buy_order_item_index,lowest_sell_order_item_index);

							//handling trader cost
							all_trader[buy_trader_id]->arr[i]->cash-=cost;
							all_trader[sell_trader_id]->arr[i]->cash+=cost;
							all_trader[buy_trader_id]->arr[i]->cash-=fee;

						}
						else{
							cost=((long long int)(lowest_sell_order->quantity)* (long long int)(lowest_sell_order->price));
							quantity=lowest_sell_order->quantity;
							//float_fee= (cost*0.01);
							fee=round(cost*0.01);
							
							matchorder_backend_sell_smaller(highest_buy_order,highest_buy_index, lowest_sell_order, lowest_sell_index,i,lowest_sell_order_item_index);
							//handling trader cost 
							all_trader[buy_trader_id]->arr[i]->cash-=cost;
							all_trader[sell_trader_id]->arr[i]->cash+=cost;
							all_trader[buy_trader_id]->arr[i]->cash-=fee;
						}

						printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%lld, fee: $%lld.\n", LOG_PREFIX, lowest_sell_order->order_id,
							lowest_sell_order->trader_id, highest_buy_order->order_id, highest_buy_order->trader_id, cost,fee);

					}
					//adding match orderids to cancel order so it cant be used by traders again
					all_trader[buy_trader_id]->cancel_arr[all_trader[buy_trader_id]->cancel_no]=highest_buy_order->order_id;
					all_trader[buy_trader_id]->cancel_no++;

					all_trader[sell_trader_id]->cancel_arr[all_trader[sell_trader_id]->cancel_no]=lowest_sell_order->order_id;
					all_trader[sell_trader_id]->cancel_no++;


					//notifying traders.
					exchange_fees=exchange_fees+fee;
					if(exchange_fds[buy_trader_id]!=-1 || all_trader_pid[buy_trader_id]!=0){
						char send_message1[128];
						memset(send_message1, 0, sizeof(send_message1));
    					sprintf(send_message1, "FILL %d %d;",highest_buy_order->order_id, quantity);
						write(exchange_fds[buy_trader_id], &send_message1,strlen(send_message1));
						kill(all_trader_pid[buy_trader_id], SIGUSR1);
					}
					if(exchange_fds[sell_trader_id]!=-1 || all_trader_pid[sell_trader_id]!=0){
						char send_message2[128];
						memset(send_message2, 0, sizeof(send_message2));
    					sprintf(send_message2, "FILL %d %d;",lowest_sell_order->order_id, quantity);
						write(exchange_fds[sell_trader_id], &send_message2,strlen(send_message2));
						kill(all_trader_pid[sell_trader_id], SIGUSR1); 
					}
					

				}
				else{
					break;
					}
			}
		}
		//---TILL HERE --//
	
	}//for each item ends here
}



void print_orderbook(){
	printf("%s	--ORDERBOOK--\n",LOG_PREFIX); 
	for(int i=0; i<products_number; i++){
		items *item=handle_duplicate_order1(all_items[i]);
		printf("%s	Product: %s; Buy levels: %d; Sell levels: %d\n", LOG_PREFIX, item->product_name ,item->buy_levels, item->sell_levels);
		
		if(item->sell_levels>0){
			
			for(int j=0; j<item->sell_levels; j++){
				printf("%s		SELL %d @ $%d (%d ", LOG_PREFIX, item->sell_orders[j]->quantity,item->sell_orders[j]->price, item->sell_orders[j]->no_of_orders);
				if(item->sell_orders[j]->no_of_orders==1){
					printf("order)\n");
				}
				else{
					printf("orders)\n");
				}
			}
		}
		
		if(item->buy_levels>0){
			
			for(int j=0; j<item->buy_levels; j++){
				printf("%s		BUY %d @ $%d (%d ", LOG_PREFIX, item->buy_orders[j]->quantity,item->buy_orders[j]->price, item->buy_orders[j]->no_of_orders);
				if(item->buy_orders[j]->no_of_orders==1){
					printf("order)\n");
				}
				else{
					printf("orders)\n");
				}
				
			}
		}
		
		free_garbage_order();
		free(item);
		item=NULL;
		
	}
	print_trader_position();
}

void test_print_all_orders(){
	for(int i=0; i<MAX_ORDERS; i++){
		if(all_orders[i]!=NULL){
			printf("order:%d trader id:%d\n", all_orders[i]->order_id, all_orders[i]->trader_id);
			printf("Order name: %s, quantity:%d @ $%d\n",all_orders[i]->product_name, all_orders[i]->quantity, all_orders[i]->price );
		}
		
	}
}

void process_message(int index){
	char send_message[128];
	char trader_message_duplicate[128];
	strcpy(trader_message_duplicate, trader_message);
	char *token=strtok(trader_message_duplicate, " ");
	if(token!=NULL && ( (strcmp(token,"BUY"))==0 || (strcmp(token,"SELL"))==0 ) ){
		int check=check_message_validity_buy_sell(index);
		if(check<0){
			memset(send_message, 0, sizeof(send_message));
    		sprintf(send_message, "INVALID;");          
    		write(exchange_fds[index], &send_message, strlen(send_message));
			kill(all_trader_pid[index], SIGUSR1);
		}
		else{
			
			memset(send_message, 0, sizeof(send_message));
    		sprintf(send_message, "ACCEPTED %d;", check);          
    		write(exchange_fds[index], &send_message, strlen(send_message));
			kill(all_trader_pid[index], SIGUSR1);
			match_order();
			print_orderbook();
		}
	}	
	else if(token!=NULL &&  (strcmp(token,"AMEND"))==0){
		int check=check_message_validity_ammend(index);
		//wrong ammend message
		if(check<0){
			memset(send_message, 0, sizeof(send_message));
    		strcpy(send_message,"INVALID;");    
    		write(exchange_fds[index], &send_message, strlen(send_message));
			kill(all_trader_pid[index], SIGUSR1);
			
		}
		else{
			memset(send_message, 0, sizeof(send_message));
    		sprintf(send_message, "AMENDED %d;", check);              
    		write(exchange_fds[index], &send_message, strlen(send_message));
			kill(all_trader_pid[index], SIGUSR1);
			match_order();
			print_orderbook();

		}
	}
	else if(token!=NULL  &&  (strcmp(token,"CANCEL"))==0){
		
		int check=check_message_validity_cancel(index);
		if(check<0){
			memset(send_message, 0, sizeof(send_message));
    		strcpy(send_message,"INVALID;");          
    		write(exchange_fds[index], &send_message, strlen(send_message));
			kill(all_trader_pid[index], SIGUSR1);
		}else{
			memset(send_message, 0, sizeof(send_message));
    		sprintf(send_message, "CANCELLED %d;",check);          
    		write(exchange_fds[index], &send_message, strlen(send_message));
			kill(all_trader_pid[index], SIGUSR1);
			print_orderbook();
		}
	}
	
}

void sigchld_handler(int sig, siginfo_t *info, void *ucontext){
	pid_t pid;
	//&status is where the exit status is stored 
	//WNOHANG returns immediately if no child process has tesminated, without it exchange will be blocked
	while( (pid= waitpid(-1, NULL, WNOHANG))> 0){
		trader_disconnect(pid);
	}
}

void sigusr1_handler(int sig, siginfo_t *info, void *ucontext) {
	//sigiinfo_t is a struct that which containes sending process id in si_pid
	pid_t trader_pid=info->si_pid;
	int trader_index=-1;
	for(int i=0; i<MAX_TRADERS; i++){
		if(trader_pid==all_trader_pid[i]){
			trader_index=i;
			break;
		}
	}
	if(trader_index!=-1){
		memset(trader_message, 0, sizeof(trader_message));
		int msg=read(trader_fds[trader_index],&trader_message, 128);
    	trader_message[msg]='\0';
		char *token= strtok(trader_message, ";");
		printf("%s [T%d] Parsing command: <%s>\n", LOG_PREFIX, trader_index, token);
		process_message(trader_index);
	}
}



int main(int argc, char **argv) {
	//need to be changed to 3.
	if (argc < 3) {
        printf("Not enough arguments\n");
        return 1;
    }
	printf("%s Starting\n", LOG_PREFIX);

	//reading product file
    FILE *products_file=fopen(argv[1],"r");
    if(products_file==NULL){
        printf("File not found or Error opening file\n");
        return 1;
    }
	
	char products_size[4];
    fgets(products_size,4, products_file);
    products_number=atoi(products_size);
	int product_no=0;
    int character_no=0;
	
    while(1){
		
        char c=fgetc(products_file);
        if(feof(products_file)){
            break;
        }
        if(c!= '\n'){
			//checking for whitespace characters
			if(isspace(c)==0){
				if(product_no>(products_number-1) || product_no>=MAX_ITEMS){
					printf("More products than mentioned: %d\n", product_no);
					exit(1);
				}
				if(character_no>16){
					printf("More than 16 characters\n");
					exit(1);
				}
				products[product_no][character_no]=c;
            	character_no++;
			}
			
        }
        else{
			if(character_no!=0){
				product_no++;
            	character_no=0;
			}
			else{
				character_no=0;
			}
            
        }   
    }
	fclose(products_file);
	//checking if all characters in product name is alpha numeric
	for(int i=0; i<products_number; i++){
		int len=strlen(products[i]);
		for(int j=0; j<(len); j++){
			if( !(isalnum(products[i][j])) ){
				printf("%d product is char %c is not alphanumeric\n", i,products[i][j]);
				exit(-1);
			}
		}
	}
	//printing all products
	printf("%s Trading %d products: ", LOG_PREFIX, products_number);
	for(int i=0; i<products_number; i++){
        printf("%s", products[i]);
		
		//making item struct
		items *new_item=malloc(sizeof(items));
		new_item->product_name=products[i];
		new_item->buy_levels=0;
		new_item->sell_levels=0;
		memset(new_item->buy_orders, 0, sizeof(new_item->buy_orders));
		memset(new_item->sell_orders, 0, sizeof(new_item->sell_orders));
		all_items[i]=new_item;

		if(i< (products_number-1)){
			printf(" ");
		}

    }
	printf("\n");

	//signal handler for sigchld
	//sigchld signal is sent everytime child process terminates
	struct sigaction sa;
    sa.sa_sigaction =sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags= SA_SIGINFO;
    if(sigaction(SIGCHLD, &sa, NULL )==-1){
        perror("Problem with signal handler");
        exit(1);
    }

	//register sigusr1 handler
	struct sigaction sa_usr1;
    sa_usr1.sa_sigaction =sigusr1_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags= SA_SIGINFO;
    if(sigaction(SIGUSR1, &sa_usr1, NULL )==-1){
        perror("Problem with signal handler");
        exit(1);
    }



	pid_t exchange= getpid();
	pid_t pid;
	for(int id=2; id<argc; id++ ){
		//creating all pipes
		char pe_exchange_pipe[50];
    	char pe_trader_pipe[50];
		//creating names of pipes
    	sprintf(pe_exchange_pipe, FIFO_EXCHANGE, (id-2));
    	sprintf(pe_trader_pipe, FIFO_TRADER, (id-2));

		//we check if the file name already exits. If it does we delete it.
		//creating file that already will fail and can cause errors.
		//named pipe can exits as long as system is up.
		if(access(pe_exchange_pipe, F_OK)==0){
			unlink(pe_exchange_pipe);
		}
		if(access(pe_trader_pipe, F_OK)==0){
			unlink(pe_trader_pipe);
		}

		if((mkfifo(pe_exchange_pipe, 0666))==0 && (mkfifo(pe_trader_pipe, 0666))==00){
			printf("%s Created FIFO %s\n", LOG_PREFIX, pe_exchange_pipe);
			printf("%s Created FIFO %s\n", LOG_PREFIX, pe_trader_pipe);
		}
		else{
			printf("Could not create pipe %d\n", id);
			exit(2);
		}

		//creating child process
		if(getpid()==exchange){
			printf("%s Starting trader %d (%s)\n", LOG_PREFIX, (id-2),  argv[id]);
			pid=fork();
			if(pid<0){
				printf("Couldnt create trader\n");
				exit(2);
			}
			else{
				//adding child process pid so we can use the array to talk to all child process later on
				all_trader_pid[id-2]=pid;
				trader *new_trader= malloc(sizeof(trader));
				new_trader->trader_id=id-2;
				new_trader->cancel_no=0;
				for(int x=0;x<products_number; x++){
					trader_item *new_item=malloc(sizeof(trader_item));
					new_item->name=products[x];
					new_item->cash=0;
					new_item->quantity=0;
					new_trader->arr[x]=new_item;
				}
				all_trader[id-2]=new_trader;

			}
		}
		char trader_id[20];
		sprintf(trader_id, "%d", (id-2));
		if(pid==0){
			if(execl(argv[id], argv[id],trader_id, NULL)==-1){
				perror("exec failed");
				exit(1);

			}
			
		}
		else if(pid>0){
			int exchange_fd=open(pe_exchange_pipe, O_WRONLY);
			int trader_fd=open(pe_trader_pipe, O_RDONLY);
			if(exchange_fd==-1 || trader_fd==-1){
				printf("problem opening pipe\n");
				exit(1);
			}
			exchange_fds[id-2]= exchange_fd;
			trader_fds[id-2]= trader_fd;
			printf("%s Connected to %s\n", LOG_PREFIX, pe_exchange_pipe);
			printf("%s Connected to %s\n", LOG_PREFIX, pe_trader_pipe);

		}

	}

	//send all traders market open message
	for(int i=0; i<MAX_TRADERS; i++){
		if(all_trader_pid[i]!=0){
			char open_msg[]="MARKET OPEN;";
			write(exchange_fds[i], &open_msg, strlen(open_msg));
			kill(all_trader_pid[i], SIGUSR1);
		}

	}

	while(1){
		pause();
	}
	
	return 0;
}