//+------------------------------------------------------------------+
//|                                             TestBufferSignal.mq4 |
//|                              Copyright 2020, Yaroslav Barabanov. |
//|                                https://t.me/BinaryOptionsScience |
//+------------------------------------------------------------------+
#property copyright "Copyright 2020, Yaroslav Barabanov."
#property link      "https://t.me/BinaryOptionsScience"
#property version   "1.00"
#property strict
#property indicator_chart_window
#property description "Indicator for testing buffer signals"
#property description "Buffer #0 - signal up (buy)"
#property description "Buffer #1 - signal down (sell)"
#property description "Buffer value 1 - signal present"
#property description "Buffer value 0 - no signal"

double test_buffer_0[];
double test_buffer_1[];

int OnInit() {
    IndicatorBuffers(2);
    IndicatorDigits(5);
    int index = 0;
    SetIndexBuffer(index++,test_buffer_0);
    SetIndexBuffer(index++,test_buffer_1);
    return(INIT_SUCCEEDED);
}

int OnCalculate(const int rates_total,
                const int prev_calculated,
                const datetime &time[],
                const double &open[],
                const double &high[],
                const double &low[],
                const double &close[],
                const long &tick_volume[],
                const long &volume[],
                const int &spread[]) {
   ArraySetAsSeries(close,false);
   ArraySetAsSeries(test_buffer_0,false);
   ArraySetAsSeries(test_buffer_1,false);
   int pos = prev_calculated-1;
   if(pos < 0) pos = 1;
   for(int i = pos; i < rates_total && !IsStopped(); i++) {
      if(close[i] > close[i - 1]) {
        test_buffer_0[i] = 0;
        test_buffer_1[i] = 1;
      } else
      if(close[i] < close[i - 1]) {
        test_buffer_0[i] = 1;
        test_buffer_1[i] = 0;
      } else {
        test_buffer_0[i] = 0;
        test_buffer_1[i] = 0;
      }
   }
   return(rates_total);
}
