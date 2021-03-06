//+------------------------------------------------------------------+
//|                                                    BinomoBot.mq4 |
//|                              Copyright 2020, Yaroslav Barabanov. |
//|                                https://t.me/BinaryOptionsScience |
//+------------------------------------------------------------------+
#property copyright "Copyright 22020, Yaroslav Barabanov."
#property link      "https://t.me/BinaryOptionsScience"
#property version   "1.30"
#property strict

#include "binomo_api.mqh"
#include "simple_label.mqh"
#include <WinUser32.mqh>
   int    hwnd=0,MT4InternalMsg=0;
#import "user32.dll"
   int PostMessageW(int hWnd, int Msg, int wParam, int lParam);
	int RegisterWindowMessageW(string lpString); 
#import
#include "xtime.mqh"

input string	s0 = "========================================";	// -

input string	price_stream_symbols_list = 
         "BTCUSD-BIN,BTCLTC-BIN,ZCRYIDX,EURUSD(OTC)";	// Array of used price stream symbols
         
input string 	price_stream_symbols_period_list = 
         "1,5";                                       	// Array of used periods for price stream
		 
input string	s1 = "===Symbols and periods for trading =====";	// -

input string	trade_symbols_list = 
         "BTCUSD-BIN,BTCLTC-BIN,ZCRYIDX";			  	// Array of symbols for trading
		 
input string 	trade_symbols_period_list = 
         "1,5";											// Array of symbol periods for trading

input string    s2 = "=== Money management parameters ========";	// -

input int       expiration = 60;                    // Expiration (second)
input double    amount = 1;                         // Bet size
input double    percentage = 1;					   	// Percentage of the bet from the deposit
input bool	    is_amount = true;				   	// Bet size (true) or percentage of the deposit (false)

input string	s3 = "=== Indicator settings =================";	// -

input string   	indicator_name = "example.ex4";      // Indicator name
input int	   	indicator_index_up = 0;			   	// Indicator buffer index for the Up / Buy signal
input int	   	indicator_index_dn = 1;			   	// Indicator buffer index for Down / Sell signal
input double   	indicator_signal_up = 1;			   	// Signal level (> =) in the buffer for the Up / Buy signal
input double   	indicator_signal_dn = 1;            	// Signal level (> =) in the buffer for the Down / Sell signal

input int      	until_bar_second = 3;               // time until the end of the bar
input int      	after_bar_second = 3;                // time after the end of the bar

input string   	s4 = "=== Bot parameters =====================";	// -	
	
input string 	pipe_name = "binomo_api_bot";    			// Named pipe name                 
input int 		timer_period = 10;                    		// Timer period (ms)
input bool		is_trade = true;							// Enable trade (true)
input bool		is_price_stream = true;						// Enable price stream (true)
                     
static datetime last_time = 0;

static string price_stream_symbol_name[];                 // список символов для обновления графиков
static string price_stream_symbol_period_str[];           // список периодов (в виде строки)
static uint price_stream_symbol_period[];                 // список периодов
static uint price_stream_number_symbol = 0;               // количество символов
static uint price_stream_number_period_symbol = 0;        // количество периодов символов

static string trade_symbol_name[];                 // 
//static string trade_symbol_indicator[];          // 
static string trade_symbol_period_str[];           // 
static uint trade_symbol_period[];                 // 
static uint trade_number_symbol = 0;               // 
static uint trade_number_period_symbol = 0;        // 

static MqlRates rates[];
static datetime opening_time_candle_signal[];    	// Время открытия бара для первого сигнала (нужно чтобы в течении одного бара был лишь один сигнал)

BinomoApi api;   // API for working with Binomo Bot

/** \brief Bot initialization
 * This function initializes everything you need to start the bot.
 */  
int bot_init();

/** \brief Create text label
 * This function creates a text label that displays balance and connection status
 */  
bool bot_make_text_label();

int OnInit() {
   return bot_init();
}

void OnTimer() {
	if(is_price_stream) {
		/* место обновление графика */
		api.update_window(
			price_stream_symbol_name, 
			price_stream_symbol_period);
	}
	
	if(!is_trade) {
		if(api.check_broker_connection_change()) {
			if(!api.check_broker_connection()) LabelTextChange(0,"text_status", "disconnected");
			else LabelTextChange(0,"text_status", "connected");
			Print("binomo bot: connecting to a broker: ",api.check_broker_connection()); 
			ChartRedraw();
		}
		
	    /* обновляем состояние API */
	    api.update(timer_period);
		return;
	}
   
	/* проверяем наличие соединения */
	if(api.connected()) {
		/* соединение есть, проверяем условия для открытия сделки */
      
		RefreshRates(); // Далее гарантируется, что все данные обновлены
      
		/* получаем время GMT */
		const datetime timestamp = TimeGMT();
		const datetime timestamp_timezone = TimeCurrent();
      
		/* проходим весь список символов */
		for(uint s = 0; s < trade_number_symbol; ++s) {
			for(uint p = 0; p < trade_number_period_symbol; ++p) {
				/* get price values */
				int err = CopyRates(trade_symbol_name[s], p, 0, 1, rates);
				if(err <= 0) continue;
				
				/* get the index in the opening_time_candle_signal array */
				const uint index = s + trade_number_period_symbol * p;

				/* Attention! the binomo has the bar closing time, not the start time */
				const int second_bar = (int)(rates[0].time - timestamp);
				//const int second_bar = (int)(timestamp - rates[0].time);
				
				//Print("second_bar: ", second_bar);
				
				/* We skip the trade entry if we are far from the bar closing time */
				bool is_skip = false;
				if(second_bar > until_bar_second && second_bar < after_bar_second) is_skip = true;
				if(is_skip) continue;
				
				/* Getting indicator values */
				double value_up = iCustom(trade_symbol_name[s], p, indicator_name, indicator_index_up, 0);
				double value_dn = iCustom(trade_symbol_name[s], p, indicator_name, indicator_index_dn, 0);

				/* check for a signal */
				if(opening_time_candle_signal[index] < rates[0].time) {
					/* if the signal is new on a new bar */
					
					if(value_up >= indicator_signal_up) {
						/* if the buffer contains a signal to enter a trade */
						
						opening_time_candle_signal[index] = rates[0].time; // remember the bar where the signal was
						api.open_deal(trade_symbol_name[s], indicator_name, BUY, expiration, amount);
						Print("binomo bot: open bo-bet ", trade_symbol_name[s]);
					} else
					if(value_dn >= indicator_signal_dn) {
						/* if the buffer contains a signal to enter a trade */
						
						opening_time_candle_signal[index] = rates[0].time; // remember the bar where the signal was
						api.open_deal(trade_symbol_name[s], indicator_name, SELL, expiration, amount);
						Print("binomo bot: open bo-bet: ", trade_symbol_name[s]);
					} 
				}
			} // for p
		} // for s
	} else {
		
		/* no connection, connect */
		if(api.connect(pipe_name)) {
			Print("Успешное соединение с ", pipe_name);
		} else {
			LabelTextChange(0,"text_status", "disconnected");
			ChartRedraw();
		}
	}
   
	/* update API state */
	api.update(timer_period);
	
	if(api.check_broker_connection_change()) {
		if(!api.check_broker_connection()) LabelTextChange(0,"text_status", "disconnected");
		else LabelTextChange(0,"text_status", "connected");
		Print("binomo: connecting to a broker: ", api.check_broker_connection()); 
		ChartRedraw();
	}
}

void OnDeinit(const int reason) {
   api.close();
   ArrayFree(price_stream_symbol_name);
   ArrayFree(price_stream_symbol_period);
   ArrayFree(price_stream_symbol_period_str);
   ArrayFree(trade_symbol_name);
   ArrayFree(trade_symbol_period);
   ArrayFree(trade_symbol_period_str);
   ArrayFree(rates);
   LabelDelete(0,"text_broker");
   LabelDelete(0,"text_status");

   ChartRedraw();
   EventKillTimer();
}

/** \brief Bot initialization
 * This function initializes everything you need to start the bot.
 */  
int bot_init() {
   string sep=",";
   ushort u_sep;
   u_sep = StringGetCharacter(sep,0);
   
   /* parse an array of currency pairs */
   StringSplit(price_stream_symbols_list, u_sep, price_stream_symbol_name);
   price_stream_number_symbol = ArraySize(price_stream_symbol_name);
   
   /* parse an array of periods */
   StringSplit(price_stream_symbols_period_list, u_sep, price_stream_symbol_period_str);
   price_stream_number_period_symbol = ArraySize(price_stream_symbol_period_str);
   ArrayResize(price_stream_symbol_period, price_stream_number_period_symbol);
   for(uint p = 0; p < price_stream_number_period_symbol; ++p) {
      price_stream_symbol_period[p] = (uint)StringToInteger(price_stream_symbol_period_str[p]);
   }
   
   /* parse an array of symbols for trading */
   StringSplit(trade_symbols_list, u_sep, trade_symbol_name);
   trade_number_symbol = ArraySize(trade_symbol_name);
   
   /* parse an array of periods for trading */
   StringSplit(trade_symbols_period_list, u_sep, trade_symbol_period_str);
   trade_number_period_symbol = ArraySize(trade_symbol_period_str);
   ArrayResize(trade_symbol_period, trade_number_period_symbol);
   for(uint p = 0; p < trade_number_period_symbol; ++p) {
      trade_symbol_period[p] = (uint)StringToInteger(trade_symbol_period_str[p]);
   }
   
   /* initialize arrays */
   ArraySetAsSeries(rates, true);
   
   ArrayResize(opening_time_candle_signal, (trade_number_period_symbol * trade_number_symbol));
   ArrayInitialize(opening_time_candle_signal, 0);

   /* initialize timestamps of trade entry locks */
   if(ArraySize(trade_symbol_period) > 0) {
       for(uint s = 0; s < trade_number_symbol; ++s) {
          for(uint p = 0; p < trade_number_period_symbol; ++p) {
              const uint index = s + trade_number_period_symbol * p;
              
              int err = CopyRates(trade_symbol_name[s], trade_symbol_period[p], 0, 1, rates);
              if(err <= 0) continue;
              opening_time_candle_signal[index] = rates[0].time;
          }
       }
   }

   /* initialize the update of the charts */
   api.init_update_window();
   
   /* initialize the timer */
   if(!EventSetMillisecondTimer(timer_period)) return(INIT_FAILED);
   if(!bot_make_text_label()) return(INIT_FAILED);
   return(INIT_SUCCEEDED);
}

/** \brief Create text label
 * This function creates a text label that displays connection status
 */  
bool bot_make_text_label() {
   const int font_size = 10;
   const int indent = 8;
   long y_distance;
   uint text_broker_width = 100;
   if(!ChartGetInteger(0,CHART_HEIGHT_IN_PIXELS,0,y_distance)) {
      Print("Failed to get the chart width! Error code = ",GetLastError());
      return false;
   }
   LabelCreate(0,"text_broker",0,indent, (int)y_distance - 2*font_size - 2*indent,CORNER_LEFT_UPPER,"binomo:","Arial",font_size,clrAliceBlue);
   LabelCreate(0,"text_status",0,indent + text_broker_width, (int)y_distance - 2*font_size - 2*indent,CORNER_LEFT_UPPER,"disconnected","Arial",font_size,clrAqua);
   ChartRedraw();
   return true;
}
