//+------------------------------------------------------------------+
//|                                            named_pipe_client.mqh |
//|                                      Copyright 2020, Elektro Yar |
//|          https://github.com/NewYaroslav/simple-named-pipe-server |
//+------------------------------------------------------------------+
#property copyright "Copyright 2020, Elektro Yar"
#property link      "https://github.com/NewYaroslav/simple-named-pipe-server"

enum ENUM_PIPE_ACCESS {
	PIPE_ACCESS_INBOUND = 1,
	PIPE_ACCESS_OUTBOUND = 2,
	PIPE_ACCESS_DUPLEX = 3,
};

enum ENUM_PIPE_MODE {
	PIPE_TYPE_RW_BYTE = 0,
	PIPE_TYPE_READ_MESSAGE = 2,
	PIPE_TYPE_WRITE_MESSAGE = 4,
};

#define PIPE_WAIT 0
#define PIPE_NOWAIT 1
#define PIPE_READMODE_MESSAGE 2

#define ERROR_PIPE_CONNECTED 535
#define ERROR_BROKEN_PIPE 109

#define INVALID_HANDLE_VALUE -1
#define GENERIC_READ  0x80000000
#define GENERIC_WRITE  0x40000000
#define OPEN_EXISTING  3
#define PIPE_UNLIMITED_INSTANCES 255
#define PIPE_BUFFER_SIZE 4096
#define STR_SIZE 255

//+------------------------------------------------------------------+
//| DLL imports                                                      |
//+------------------------------------------------------------------+
#import "kernel32.dll"
int CreateNamedPipe(string pipeName,int openMode,int pipeMode,int maxInstances,int outBufferSize,int inBufferSize,int defaultTimeOut,int security);
int WaitNamedPipeW(string lpNamedPipeName,int nTimeOut);
bool ConnectNamedPipe(int pipeHandle,int overlapped);
bool DisconnectNamedPipe(int pipeHandle);
int CreateFileW(string name,int desiredAccess,int SharedMode,int security,int creation,int flags,int templateFile);
int WriteFile(int fileHandle,short &buffer[],int bytes,int &numOfBytes,int overlapped);
int WriteFile(int fileHandle,char &buffer[],int bytes,int &numOfBytes,int overlapped);
int WriteFile(int fileHandle,MqlTick &outgoing,int bytes,int &numOfBytes,int overlapped);
int WriteFile(int fileHandle,int &var,int bytes,int &numOfBytes,int overlapped);
int ReadFile(int fileHandle,short &buffer[],int bytes,int &numOfBytes,int overlapped);
int ReadFile(int fileHandle,char &buffer[],int bytes,int &numOfBytes,int overlapped);
int ReadFile(int fileHandle,MqlTick &incoming,int bytes,int &numOfBytes,int overlapped);
int ReadFile(int fileHandle,int &incoming,int bytes,int &numOfBytes,int overlapped);
int CloseHandle(int fileHandle);
int GetLastError(void);
int FlushFileBuffers(int pipeHandle);
bool SetNamedPipeHandleState(int fileHandle,int &lpMode, int lpMaxCollectionCount,int lpCollectDataTimeout);
bool PeekNamedPipe(int fileHandle, int buffer, int bytes, int bytesRead, int &numOfBytes, int bytesLeftThisMessage);
#import

/** \brief Класс клиента именованного канала
 */
class NamedPipeClient {
private:
	int hPipe; // хэндл канала
	string pipeNumber;
	string pipeNamePrefix;
	int BufferSize;

protected:

public:

	/** \brief Конструктор класса
	 */
	NamedPipeClient(){
		pipeNamePrefix="\\\\.\\pipe\\";
		BufferSize = PIPE_BUFFER_SIZE;
		hPipe = INVALID_HANDLE_VALUE;
		int err = kernel32::GetLastError();
	}
	
	/** \brief Деструктор класса
	 */
	~NamedPipeClient() {
		if(hPipe!=INVALID_HANDLE_VALUE) CloseHandle(hPipe);
	}
	
	/** \brief Установить размер буфера
	 * \param size размер буфера
	 */ 
	void set_buffer_size(int size) {
	   BufferSize = size;
	}

	/** \brief Открывает канал, открытый ранее
	 * \param name Имя именнованного канала
	 * \return Вернет true - если успешно, иначе false
	 */ 
	bool open(string name) {
		string fullPipeName = pipeNamePrefix + name;

		if(hPipe == INVALID_HANDLE_VALUE) {
			if(WaitNamedPipeW(fullPipeName, 5000) == 0) {
				// Print("Pipe "+fullPipeName+" busy.");
				return false;
			}

			hPipe = CreateFileW(
				fullPipeName, 
				(int)(GENERIC_READ | GENERIC_WRITE), 
				0, 
				NULL, 
				OPEN_EXISTING, 
				0, 
				NULL);
			if(hPipe == INVALID_HANDLE_VALUE) {
				Print("Pipe open failed");
				return false;
			}
			
			/* устанавливаем режим чтения
          * Клиентская сторона именованного канала начинается в байтовом режиме,
          * даже если серверная часть находится в режиме сообщений.
          * Чтобы избежать проблем с получением данных,
          * установите на стороне клиента также режим сообщений.
          * Чтобы изменить режим канала, клиент канала должен
          * открыть канал только для чтения с доступом
          * GENERIC_READ и FILE_WRITE_ATTRIBUTES
          */
         int mode = PIPE_READMODE_MESSAGE;
         bool success = SetNamedPipeHandleState(
            hPipe,
            mode,
            NULL,     // не устанавливать максимальные байты
            NULL);    // не устанавливайте максимальное время
         
         if(!success) {
            Print("SetNamedPipeHandleState failed");
            CloseHandle(hPipe);
				return false;
         }

		}
		return true;
	}

	/** \brief Закрывает хэндл канала
	 * \return 0 если успешно, иначе ненулевое значение  
	 */ 
	int close() {
		int err = CloseHandle(hPipe);
		hPipe = INVALID_HANDLE_VALUE;
		return err;
	}

	/** \brief Сбрасывает буфер канала  
	 */
	void flush() {
		FlushFileBuffers(hPipe);
	}
	
	/** \brief Записывает строку формата ANSI в канал
	 * \param message Строка, содержащее сообщение
	 * \return Вернет true, если запись прошла успешно
	 */
	bool write(string message) {
		int bytesToWrite, bytesWritten;
		uchar ANSIarray[];
		bytesToWrite = StringToCharArray(message, ANSIarray);
		WriteFile(
			hPipe,
			ANSIarray,
			StringLen(message),
			bytesWritten,
			0);
		if(bytesWritten != StringLen(message)) return false;
		return true;
	}
   
	/** \brief Читает строку формата ANSI из канала 
	 * \return строка в формате Unicode (string в MQL5)
	 */
	string read() {
		string ret;
		uchar ANSIarray[STR_SIZE];
		int bytesRead;
		ReadFile(
			hPipe,
			ANSIarray,
			STR_SIZE,
			bytesRead,
			0);
		if(bytesRead != 0) ret = CharArrayToString(ANSIarray,0,bytesRead);
		return ret;
	}
	
	/** \brief Получить количество байтов для чтения
	 * \return количество байтов
	 */
	int get_bytes_read() {
	   int bytes_to_read = 0;
      PeekNamedPipe(
         hPipe,
         NULL,
         0,
         NULL,
         bytes_to_read,
         NULL);
      return bytes_to_read;
	}

	/** \brief Возвращает имя канала
	 * \return трока с именем канала
	 */
	string get_pipe_name() {
		return pipeNumber;
	}
};
