//+------------------------------------------------------------------+
//|                                                   binomo_api.mqh |
//|                         Copyright 2019-2020, Yaroslav Barabanov. |
//|                                https://t.me/BinaryOptionsScience |
//+------------------------------------------------------------------+
#property copyright "Copyright 2019-2020, Yaroslav Barabanov."
#property link      "https://t.me/BinaryOptionsScience"
#property strict

#include "named_pipe_client.mqh"
#include "hash.mqh"
#include "json.mqh"

#include <WinUser32.mqh>
#import "user32.dll"
   int PostMessageW(int hWnd, int Msg, int wParam, int lParam);
	int RegisterWindowMessageW(string lpString); 
#import

class BinomoApi {
private:
   NamedPipeClient pipe;
   bool is_connected;
   bool is_broker_connected;
   bool is_broker_prev_connected;
   bool is_demo;
   
   int tick;                     // ticks for counting ping sending time

   /* data for redrawing the chart */
   int hwnd;
   int MT4InternalMsg;
   datetime last_update_window_time;
   bool symbol_error_flag[];
   
public:

    enum ENUM_BO_ORDER_TYPE {
       	BUY = 0,
       	SELL = 1,
    };

    BinomoApi() {
        pipe.set_buffer_size(2048);
        is_connected = false;
        is_broker_connected = false;
        is_broker_prev_connected = false;
        tick = 0;

        hwnd=0;
        MT4InternalMsg=0;
        last_update_window_time = 0;
    }
   
    ~BinomoApi() {
        close();
    }
   
    bool connect(string api_pipe_name) {
        if(is_connected) return true;
        is_connected = pipe.open(api_pipe_name);
        return is_connected;
    }
   
    /** \brief Connection status
     * \return Will return true if there is a connection to the bot
     */ 
    bool connected() {
        return is_connected;
    }
   
    bool check_connection() {
        return is_broker_connected;
    } 
   
    bool check_connection_change() {
        if(is_broker_prev_connected != is_broker_connected) {
            is_broker_prev_connected = is_broker_connected;
            return true;
        }
        return false;
    } 
    
    /** \brief Check the connection with the broker
     * \return Returns true if the connection to the broker is established
     */
    bool check_broker_connection() {
        return is_broker_connected;
    } 
    
    /** \brief Check the change in the state of the connection with the broker
    * \return Returns true if the broker connection state has changed
    */
    bool check_broker_connection_change() {
        if(is_broker_prev_connected != is_broker_connected) {
            is_broker_prev_connected = is_broker_connected;
            return true;
        }
        return false;
    } 
   
   /** \brief Place a bet
    * \param user_symbol Symbol name
    * \param user_note The note
    * \param user_direction Bet direction (BUY - '1', SELL - '-1')
    * \param user_expiration Expiration (in seconds)
    * \param user_amount Bet size
    * \return Returns true if the method succeeds
    */ 
    bool open_deal(const string user_symbol, const string user_note, const int user_direction, int user_expiration, const double user_amount) {
        if(!is_connected) return false;
        if(!is_broker_connected) return false;
        if(user_direction != BUY && user_direction != SELL) return false;
        string json_body;
        string str_direction = user_direction == BUY ? "BUY" : "SELL";
        json_body = StringConcatenate(json_body,"{\"contract\":{\"s\":\"",user_symbol,"\",\"note\":\"",user_note,"\",\"dir\":\"",str_direction,"\",\"a\":",user_amount,",");
        json_body = StringConcatenate(json_body,"\"dur\":",user_expiration);
        json_body = StringConcatenate(json_body,"}}");
        if(pipe.write(json_body)) return true;
        close();
        return false;
    }
   
   /** \brief Refresh API state
    * \param delay Delay between function calls
    */
   void update(int delay) {
      if(!is_connected) return;
      //Print("update");
      const int MAX_TICK = 10000;
      tick += delay;
      if(tick > MAX_TICK) {
         //Print("ping");
         tick = 0;
         string json_body = "{\"ping\":1}";
         if(!pipe.write(json_body)) {
            close();
         }
      }
      if(pipe.get_bytes_read() > 0) {
         string body = pipe.read();
         //Print("body: ", body);
         
         /* парсим json сообщение */
         JSONParser *parser = new JSONParser();
         JSONValue *jv = parser.parse(body);
         if(jv == NULL) {
            Print("error:"+(string)parser.getErrorCode() + parser.getErrorMessage());
         } else {
            if(jv.isObject()) {
               JSONObject *jo = jv;     

               /* проверяем сообщение ping */
               int itemp = 0;
               if(jo.getInt("ping", itemp)){
                  //Print("ping: ",itemp);
                  string json_body = "{\"pong\":1}";
                  if(!pipe.write(json_body)) {
                     close();
                  }
               }
               
               /* получить флаг демо аккаунта */
               itemp = 0;
               if(jo.getInt("demo", itemp)){
                  is_demo = (itemp == 1);
               }
               
               /* проверяем состояние соединения */
               if(jo.getInt("connection", itemp)){
                  if(itemp == 1) is_broker_connected = true;
                  else is_broker_connected = false;
                  //Print("connection: ",itemp);
               }
            }
            delete jv;
         }
         delete parser;
      }
   }
   
   /** \brief Close connection with bot
    */
   void close() {
      if(is_connected) pipe.close();
      is_connected = false;
      is_broker_connected = false;
      is_broker_prev_connected = false;
      tick = 0;

      if(ArraySize(symbol_error_flag) != 0) {
         ArrayFree(symbol_error_flag);
      }
   }
   
   /** \brief Initialize window charts
    */
   void init_update_window() {
      if(MT4InternalMsg == 0) MT4InternalMsg = RegisterWindowMessageW("MetaTrader4_Internal_Message");
   }
   
   /** \brief Refresh window charts
    */
   void update_window(string &_symbol_name[], uint &_symbol_period[]) {
      const uint _num_symbol = ArraySize(_symbol_name);
      const uint _num_period_symbol = ArraySize(_symbol_period);
      const uint num_symbol_x_num_period_symbol = _num_period_symbol * _num_symbol;
      
      if(ArraySize(symbol_error_flag) != num_symbol_x_num_period_symbol) {
         ArrayResize(symbol_error_flag, num_symbol_x_num_period_symbol);
         for(uint i = 0; i < num_symbol_x_num_period_symbol; ++i) {
            symbol_error_flag[i] = false;
         }
      }
       
      datetime cur_time = TimeGMT();
      //if((cur_time - last_update_window_time) >= 0) {
      {   
         uint index = 0;
         for(uint s = 0; s < _num_symbol; ++s) {
            for(uint p = 0; p < _num_period_symbol; ++p) {
               hwnd = WindowHandle(_symbol_name[s], _symbol_period[p]);
               if(hwnd !=0) {
                  PostMessageW(hwnd, WM_COMMAND, 33324, 0);
                  PostMessageW(hwnd, MT4InternalMsg, 2, 1);
                  symbol_error_flag[index] = false;
               } else if(!symbol_error_flag[index]) {
                  Print("Отсутствует график: ",_symbol_name[s], " период: ", _symbol_period[p]);  
                  symbol_error_flag[index] = true;
               }
               ++index;
            }
         }
         last_update_window_time = cur_time;
      }
   }
};

