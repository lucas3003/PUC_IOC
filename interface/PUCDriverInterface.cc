#include <asynDriver.h>
#include <asynOctet.h>
#include <stdlib.h>

#include "devStream.h"
#include "StreamBusInterface.h"
#include "StreamError.h"
#include "StreamBuffer.h"

#include <Comando.h>

#include <epicsAssert.h>

extern "C" {
	static void processRequest(asynUser*);
	static void timeoutCallback(asynUser*);
}

enum IoAction {
    None, Lock, Write, Read, AsyncRead, AsyncReadMore,
    ReceiveEvent, Connect, Disconnect
};

class PUCDriverInterface : StreamBusInterface
{
	asynUser* pasynUser;
	IoAction ioAction;
	double lockTimeout, sendTimeout, readTimeout, replyTimeout;	
	long expectedLength;
	const char * sendBuffer;
	char temporaryBuffer[5];
	size_t sendSize;
	StreamBuffer inputBuffer;
	bool first;
	
	unsigned char * enderecamento;
	unsigned char * cabecalho;
	unsigned char * carga;	
	int tam, bytesToRead;
	
	asynCommon* pasynCommon;	
	void* pvtCommon;
	
	asynOctet* pasynOctet;
	void* pvtOctet;
	
	Comando com;
		
   PUCDriverInterface(Client* client);
   ~PUCDriverInterface(); 
   
   friend void processRequest(asynUser*);
   friend void timeoutCallback(asynUser*);
   
   void initAsynOctet(asynInterface * i);
   void initAsynCommon(asynInterface * i);
   
   bool connectToBus(const char* busname, int addr);
   bool connectToAsynPort();
   
   void lockHandler();
   void writeHandler();
   void readHandler();
   
   bool verifySupport();
   
    asynQueuePriority priority() {
        return static_cast<asynQueuePriority>
            (StreamBusInterface::priority());
    }
    
    bool lockRequest(unsigned long lockTimeout_ms);
    bool unlock();
    bool writeRequest(const void* output, size_t size,unsigned long writeTimeout_ms);
    bool readRequest(unsigned long replyTimeout_ms, unsigned long readTimeout_ms, long expectedLength, bool async);
    
    bool acceptEvent(unsigned long mask, unsigned long replytimeout_ms){return false;}; //So e chamado se supportsEvent retornar true. Nao implementado
    bool supportsEvent(); 
    bool supportsAsyncRead(){return false;}; //Nao implementado
    bool connectRequest(unsigned long connecttimeout_ms){return false;}; //Nao implementado
    bool disconnectRequest(){return false;}; //Nao implementado
    void finish(){return;}; //Nao implementado    
    //
    
public:
	//Inibe duas instancias da mesma classe. Singleton.
	 static StreamBusInterface* getBusInterface(Client* client, const char* busname, int addr, const char* param);
};

RegisterStreamBusInterface(PUCDriverInterface);


PUCDriverInterface::PUCDriverInterface(Client* client) : StreamBusInterface(client)
{
	first = true;
	bytesToRead = 4;
	
	pasynUser = pasynManager->createAsynUser(processRequest, timeoutCallback);
	assert(pasynUser);
	pasynUser->userPvt = this;
}

PUCDriverInterface::~PUCDriverInterface()
{
	//Destroy
}

StreamBusInterface * PUCDriverInterface:: getBusInterface(Client* client, const char* busname, int addr, const char*)
{
	PUCDriverInterface * interface = new PUCDriverInterface(client);
	
	if(interface->connectToBus(busname, addr))
	{
		debug("Interface criada\n");
		return interface;
	}
	
	delete interface;
	return NULL;
}

bool PUCDriverInterface:: supportsEvent()
{	
	return false;
}

void PUCDriverInterface :: initAsynOctet(asynInterface * i)
{
	pasynOctet = static_cast<asynOctet*>(i->pinterface); 
	pvtOctet = i->drvPvt;
}

void PUCDriverInterface :: initAsynCommon(asynInterface * i)
{
    pasynCommon = static_cast<asynCommon*>(i->pinterface);
    pvtCommon = i->drvPvt;	
}


bool PUCDriverInterface :: connectToBus(const char * busname, int addr)
{	
	if(pasynManager->connectDevice(pasynUser, busname, addr) != asynSuccess)
	{
		error("Falha ao conectar com dispositivo\n");
		return false;
	}
	
	return verifySupport();
}

bool PUCDriverInterface :: lockRequest(unsigned long lockTimeout_ms)
{
	//Considerar colocar parte deste trecho em uma funcao, pois eh feita mesma coisa no connectToAsynPort
	
   int connected;
   asynStatus status;		
   debug("Requerido acesso exclusivo ao dispositivo\n");	
   
   lockTimeout = lockTimeout_ms ? lockTimeout_ms*0.001 : -1.0;
   ioAction = Lock;
   
   status = pasynManager->isConnected(pasynUser, &connected);
   
   if(status != asynSuccess)
   {
	   error("Nao foi possivel checar a conexao com a interface asynUser\n");
	   return false;
   }
   
   //
   
   status = pasynManager->queueRequest(pasynUser, connected ? priority() : asynQueuePriorityConnect , lockTimeout);
   
   if(status != asynSuccess)
   {
	   error("Nao foi possivel possivel o acesso ao dispositivo\n");
	   return false;
   }
   
   return true;   
}

bool PUCDriverInterface :: connectToAsynPort()
{
	int connected;
	asynStatus status;
	
    debug("Conexao com a porta assincronamente\n");	
   
    status = pasynManager->isConnected(pasynUser, &connected);
   
   if(status != asynSuccess)
   {
	   error("Nao foi possivel checar a conexao com a interface asynUser\n");
	   return false;
   }
   
   //Se entrar no if, nao esta conectado ainda. Portanto, conectar.
   if(!connected)
   {
	   status = pasynCommon->connect(pvtCommon, pasynUser);
	   
	   if(status != asynSuccess)
	   {
		   error("Comunicacao com a porta falhou\n");
		   return false;
	   }
	   else debug("Comunicacao com a porta feita com sucesso\n");	   
   }
   
   return true;	
}

//Funcao chamada pela interface asynManager, quando ja tivermos exclusivo acesso ao hardware
void PUCDriverInterface :: lockHandler()
{
	int connected;
	pasynManager->blockProcessCallback(pasynUser, false);
	connected = connectToAsynPort();
	
	if(connected)
	   lockCallback(StreamIoSuccess);
	else
	   lockCallback(StreamIoFault);	
}

//Funcao chamada quando ja nao precisamos de acesso exclusivo ao hardware
bool PUCDriverInterface :: unlock()
{
	debug("Excluindo acesso exclusivo ao hardware\n");
	pasynManager->unblockProcessCallback(pasynUser, false);
	return true;
}

//Faz requisicao para escrever
bool PUCDriverInterface :: writeRequest(const void* output, size_t size, unsigned long writeTimeout_ms)
{	
	debug("Requisicao de escrita sendo feita\n");	
	asynStatus status;
	
	sendBuffer = com.sendPacket(StreamBuffer(output, size).expand()(), &sendSize);
	
	debug(sendBuffer);
	
	sendTimeout = writeTimeout_ms*0.001;
	ioAction = Write;
	status = pasynManager->queueRequest(pasynUser, priority(), sendTimeout);
	debug("Feito o queuerequest\n");
	
	if(status != asynSuccess)
	{
		error("Erro ao escrever requisicao\n");
		return false;
	}
	
	debug("Requisicao de escrita feita com sucesso");
	return true;
	
}

//Requisicao de leitura
bool PUCDriverInterface :: readRequest(unsigned long replyTimeout_ms, unsigned long readTimeout_ms, long _expectedLength, bool async)
{
	asynStatus status;
	readTimeout = readTimeout_ms*0.001;
	replyTimeout = replyTimeout_ms*0.001;
	expectedLength = _expectedLength;
	
	double queueTimeout;
	
	//A implementar: Verificar variavel asyn
	
	ioAction = Read;
	queueTimeout = replyTimeout;
	
	status = pasynManager->queueRequest(pasynUser, priority(), queueTimeout);
	
	if(status != asynSuccess)
	{
		error("Erro ao colocar requisicao de leitura na fila");
		return false;
	}
	
	debug("Requisicao de leitura feita.\n");
	return true;
}



//Funcao chamada quando ja temos permissao para escrever no hardware. Chamado pela interface asynManager,
//atraves da funcao handleRequest
void PUCDriverInterface :: writeHandler()
{
	debug("Permissao concedida para escrever no hardware\n");
	asynStatus status;
	
	size_t written = 0;
	pasynUser->timeout = sendTimeout;
	
	//Limpa o buffer
	status = pasynOctet->flush(pvtOctet, pasynUser);
	
	if(status != asynSuccess)
	{
		error("Erro ao limpar o buffer de saida\n");
		return;
	}
	
	//Escreve, supondo que nao ha terminator
	status = pasynOctet->write(pvtOctet, pasynUser, sendBuffer, sendSize, &written);	
	
	//Se entrar no if, tudo ocorreu da maneira esperada
	if(status == asynSuccess)
	{
		debug("Escrito no hardware com sucesso\n");
		//Verifica se escreveu tudo necessario
		sendBuffer += written;
		sendSize   -= written;
		
		if(sendSize > 0)
		{
			status = pasynManager->queueRequest(pasynUser, priority(), lockTimeout);
			
			if(status != asynSuccess)
			{
				error("Erro ao colocar solicitacao na fila\n");				
			}
			
			return;			
		}
		writeCallback(StreamIoSuccess);
		return;		
	}
	//Melhorar verificacao de erros nessa parte
	else
	{
		error("Erro ao escrever no hardware\n");
		return;
	}	   
}

void PUCDriverInterface :: readHandler()
{
	debug("Permissao concedida para ler do hardware\n\n");
	//unsigned char * buffer = (unsigned char *) malloc(64*sizeof(char)); //= inputBuffer.clear().reserve(bufferSize);
	unsigned char * buffer = (unsigned char *) inputBuffer.clear().reserve(64*sizeof(char));
	pasynUser->timeout = replyTimeout;
	size_t received;
	int eomReason;
	asynStatus status;	
	unsigned char temp_tam;
	
	if(first) bytesToRead = 4;
	status = pasynOctet->read(pvtOctet, pasynUser, (char *) buffer, bytesToRead, &received, &eomReason);
	debug("\n");
	if(status == asynSuccess)
	{
		if(first)
		{
	        enderecamento =  (unsigned char*) malloc(2*sizeof(*enderecamento));
	        enderecamento[0] =  buffer[0];	        
	        enderecamento[1] = buffer[1];
	        
	        cabecalho = (unsigned char*) malloc(2*sizeof(*cabecalho));
	        cabecalho[0] = buffer[2];
	        cabecalho[1] = buffer[3];
	        temp_tam = cabecalho[1];	        
	        
			if((temp_tam >> 7) == 1)
			{
				temp_tam = temp_tam & 0x7F; //Seta o primeiro bit como 0
				tam = (128*(1+temp_tam))+2;
			}
			else tam = temp_tam;	       
			
			//tam = cabecalho[1];
	        
			carga = (unsigned char*) malloc(tam+1*sizeof(*carga));
			bytesToRead = tam+1;
			
			//debug("TAMANHO DA CARGA = %d\n", tam);
			//debug("BYTES TO READ = %d\n", bytesToRead);
			
			first = false;
		}
		else
		{							
			carga = buffer;
			char * processed = com.receivedPacket(enderecamento, cabecalho, carga, tam);							
			int i = 0;	
			
			while(processed[i] != '\0') i++;
			
			debug("I = %d\n", i);
            readCallback(StreamIoEnd, processed, i);			            			
			
			//debug(processed);			
			first = true;
		}		
	}
	
	
	return;
}

bool PUCDriverInterface :: verifySupport()
{
	//Verifica suporte com as interfaces: asynCommon, asynOctet e se possui interface com asyInt32 ou asynUInt32
	
	asynInterface * pasynInterface;
		
	//Verifica suporte com asyncommon, que é uma interface que possui metodos para conectar e desconectar do dispositivo	
	pasynInterface = pasynManager->findInterface(pasynUser, asynCommonType, true);
	
	if(!pasynInterface)
	{
		error("asynCommon nao suportado\n");
		return false;
	}
	
	initAsynCommon(pasynInterface);
	
	//Verifica suporte com asynoctet, que é uma interface que possui metodos para comunicação baseada em mensagens	
	
	pasynInterface = pasynManager->findInterface(pasynUser, asynOctetType, true);
	
	if(!pasynInterface)
	{
		error("asynOctet nao suportado\n");
		return false;
	}
	
	initAsynOctet(pasynInterface);
	
	return true;
}

void processRequest(asynUser* pasynUser)
{
		PUCDriverInterface* interface = static_cast<PUCDriverInterface*>(pasynUser->userPvt);
		debug("Callback da funcao pasynManager->queueRequest(). Redireciona a acao para os metodos especificos\n");
		
		switch(interface->ioAction)
		{
			case None:
			   break;
			     
			case Lock:
			   interface->lockHandler();
			   break;
			   
			case Write:
				interface->writeHandler();
				
	        case AsyncRead:
	        case Read:
	           interface->readHandler();
	        
	        //Funcoes ainda nao implementadas	   
			case AsyncReadMore:						   
			case Connect:
			case Disconnect:
				break;			
		}
}

void timeoutCallback(asynUser* pasynUser)
{
	PUCDriverInterface* interface = static_cast<PUCDriverInterface*> (pasynUser->userPvt);
	debug("Tempo de espera excedido!");
	
	switch(interface->ioAction)
	{
		case Lock:
			interface->lockCallback(StreamIoTimeout);
			break;
			
		case Write:
			interface->writeCallback(StreamIoTimeout);
			break;
			
		case Read:
			interface->readCallback(StreamIoTimeout);
			break;
	}
	
	return;
}
