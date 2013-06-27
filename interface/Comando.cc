#include <string.h>
#include <stdlib.h>
#include <cstring>
#include <Comando.h>
#include "StreamError.h"
#include <string.h>
#include <stdint.h>
#include <string>

/*
 * Prox: String que tem todo o comando vindo do StreamDevice. Porem, essa string vai sendo "picotada" durante os processos,
 * para nao haver duplicacao de processamento.
 * 
 * Processed: String que tera a string que vai desde o primeiro caractere de prox ate o primeiro espaco
*/
void Comando :: process(char **prox, char **processed)
{
    int i = ateEspaco(*prox);
    debug("ateEspaco = %d\n", i);	
    *processed = (char *) malloc(i);    
    sscanf(*prox, "%s", *processed);
    debug("Depois do malloc e do sscanf\n");
    *prox = strstr(*prox, " ")+1;		    
}

int Comando :: ateEspaco(char * str)
{
	int i = 0;
	
    while(1)
    {
		if(str[i] == ' ') break;
		i++;
		
		if(i == strlen(str)-1) { i++; break; };		  
	}	
	
	return i;
}

//Observacao: O checksum esta vindo junto com a carga
bool Comando :: verificaChecksum(unsigned char * enderecamento, unsigned char * cabecalho, unsigned char * carga, int tam)
{
	int i;
	unsigned char conteudo_total = 0, check;
	
	
	//Como o enderecamento e o cabecalho possuem o mesmo tamanho, da para realizar em um so for
	for(i = 0; i < 2; i++)
	{	
		conteudo_total += enderecamento[i];
		conteudo_total += cabecalho[i];
	}
	
	
	for(i = 0; i < tam; i++)
		conteudo_total += carga[i];	
	
	check = conteudo_total + carga[tam];
	
	debug("Checksum correto: %d", 256-conteudo_total);
	
	if(check) return false;	
	return true;
}


char * Comando :: sendPacket(char* fromStream, size_t * tam)
{
   debug("sendPacket chamado\n");
   debug(fromStream);
	
    char *rec;
    prox = strstr(fromStream, " ");
    
    rec = (char *) malloc(ateEspaco(fromStream));
    sscanf(fromStream, "%s", rec);
    prox++;
    
    debug("\nCOMANDO ENVIADO: t%st\n\n", rec);
    
   if(strcmp(rec, "LER_VARIAVEL") == 0) 
   {
	   comando = LER_VARIAVEL;
	   *tam = 6*sizeof(char);
	   lerVariavel();
   }
   else if(strcmp(rec, "ESCREVER_VARIAVEL") == 0) 
   {	  
	   comando = ESCREVER_VARIAVEL;
	   *tam = escreverVariavel();
   }
   else if(strcmp(rec, "TRANSMITIR_BLOCO_CURVA") == 0)
   {	   
	   comando = TRANSMITIR_BLOCO_CURVA;
	   *tam = transmitirBlocoCurva();
   }
   else if(strcmp(rec, "BLOCO_CURVA") == 0)
   {
	   comando = BLOCO_CURVA;
	   *tam = blocoCurva();
   }
   
   return buf;    
}


char * Comando :: receivedPacket(unsigned char* enderecamento,unsigned char* cabecalho,unsigned char* carga, int tam)
{	
	//Depurando o que esta chegando no pacote
	debug("%d\n",enderecamento[0]);
	debug("%d\n",enderecamento[1]);
	debug("%d\n",cabecalho[0]);
	debug("%d\n",cabecalho[1]);
	debug("%d\n",carga[0]);
	debug("%d\n",carga[1]);
	debug("%d\n",carga[2]);
	debug("%d\n",carga[3]);			
	//
	
	if(cabecalho[0] == BLOCO_CURVA)
	{
		debug("BLOCO_CURVA\n");
		return blocoCurva(cabecalho, carga);
	}
	else if(verificaChecksum(enderecamento, cabecalho, carga, tam))		
	{
		if(cabecalho[0] == LEITURA_VARIAVEL)
		{
			return leituraVariavel(enderecamento, carga, tam);
		}
		else if(cabecalho[0] == OK_COMMAND)
		{
			return okCommand(enderecamento);
		}		
		else
		{
			error("Comando invalido");
			return "Comando invalido";
		}
	}
	else
	{
		checksumInvalido(carga);
	}	
}


//Fazer sobrecarga deste metodo para atender a comunicacao Mestre -> No
char * Comando :: blocoCurva(unsigned char* cabecalho,unsigned char* carga)
{
	double volts;	
	unsigned int conteudo = 0;			
	unsigned char temp_tam = cabecalho[1];
	unsigned char id, offset;
	int tam, i, j, tamResult = 11, numBytes = 2;
	//char * result = "BLOCO_CURVA 1 4";
	char result[128000] = "BLOCO_CURVA"; //= "BLOCO_CURVA";
	char str[30];
	
	//Considerar colocar esse bloco em um metodo separado
	if((temp_tam >> 7) == 1)
	{
		temp_tam = temp_tam & 0x7F; //Seta o primeiro bit como 0
		tam = (128*(1+temp_tam))+2;
	}
	else tam = temp_tam;
	
	////////////////////////////////////////////////////
	
	if(tam != 16386) 
	{
		error("Curva de tamanho %d e invalido", tam);
		return "Curva invalida";
	}	
	
	id = carga[0];
	offset = carga[1];
	
	sprintf(result, "%s %d %d",result, id, offset);		
	
	debug("Carga[%d] = %d\n",16000,carga[16000]);

	for(i = 2; i < tam; i+=numBytes)
	{		
		conteudo = 0;
		volts = 0;
		
		for(j = 0; j < numBytes; j++)
		{
			conteudo = conteudo << 8;
			conteudo += carga[i+j];			
		}

		volts = ((20*conteudo)/65535.0)-10;		
		sprintf(result, "%s %.5f",result, volts);		
	}
	
	return result;
}

char * Comando :: checksumInvalido(unsigned char* carga)
{
	error("Checksum invalido");
	return "Checksum invalido";
}

char * Comando :: okCommand(unsigned char * enderecamento)
{
	char * result;	
	asprintf(&result, "OK %u", enderecamento[1]);
	
	return result; 
}

char * Comando :: leituraVariavel(unsigned char * enderecamento, unsigned char* carga, int tam)
{
			char * result;	
			int i;			
			double volts;
			unsigned int conteudo = 0;			
			char str[30];
								
			for(i = 0; i < tam; i ++)
			{
				conteudo = conteudo << 8;
				conteudo += carga[i];
			}
			
			sprintf(str, "%f\n", volts);
			
			//result = (char *) malloc(sizeof(str) + sizeof("LEITURA_VARIAVEL "));			
			volts = ((20*conteudo)/262143.0)-10;			
			asprintf(&result, "LEITURA_VARIAVEL %u %f", enderecamento[1], volts);
						
			return result;	
}

int Comando :: blocoCurva()
{
	char * destino, * id, * offset , * rec;
	int checksum = 0, i, j, ibuf;
	unsigned int conteudo;
	double tensao;
	
	//Destino 
	process(&prox, &rec);
	destino = rec;
	
	//ID
	process(&prox, &rec);
	id = rec;
	
	//Offset
	process(&prox, &rec);
	offset = rec;

    debug("blocoCurva()\n");	
	
	buf = (char *) malloc(16390*sizeof(char));
	
	buf[0] = atoi(destino);
	buf[1] = 0;
	buf[2] = BLOCO_CURVA;
	buf[3] = 0xFF;
	buf[4] = atoi(id);
	buf[5] = atoi(offset);
	ibuf = 6; //Indice do buffer
	
	debug("PROX = %s , REC = %s\n", prox, rec);
	process(&prox, &rec);
	
	//8192 pontos em cada offset
	for(i = 0; i < 8192; i++)
	{
		if(strlen(prox) == 0)
		{			
			buf[ibuf++] = 0;
			buf[ibuf++] = 0;
		}
		else
		{
			debug("DEBUG %d\n", i);
			debug("ANTES DO PROCESS: PROX = t%st , REC = %s\n", prox, rec);
			process(&prox, &rec);
			debug("DEPOIS DO PROCESS: PROX = t%st , REC = %s\n", prox, rec);
			
			tensao = atof(rec);
			debug("Tensao: %f\n", tensao);
			
			conteudo = (unsigned int) ((tensao+10)*65535)/20.0;
			debug("Conteudo: %u\n", conteudo);
			
			buf[ibuf++] = (conteudo >> 8) & 0xFF;
			debug("buf[%d] = %u\n",ibuf-1, buf[ibuf-1]);
			buf[ibuf++] = conteudo & 0xFF;
			debug("buf[%d] = %u\n",ibuf-1, buf[ibuf-1]);		
		}								
	}
	
	buf[16389] = 0;
	
	return 16390; //2 de enderecamento, 2 de cabecalho, 16386 de carga util e 0 de checksum
}


int Comando :: escreverVariavel()
{
	   char * destino, * tamanho, * rec;	 
	   int i, result;
	   int checksum = 0;
	   double tensao;
	   unsigned int conteudo;	  
	  
	   //Destino
	   process(&prox, &rec);	   	   
	   destino = rec;	   
	   
	   //Tamanho
	   process(&prox, &rec);
	   tamanho = rec;
	   
	   buf = (char *) malloc(5*sizeof(char) + (atoi(tamanho)+1)*sizeof(char));
	   result = 5*sizeof(char) + (atoi(tamanho)+1)*sizeof(char);
	   
	   buf[0] = atoi(destino);
	   buf[1] = 0;
	   buf[2] = ESCREVER_VARIAVEL;
	   buf[3] = atoi(tamanho)+1;
	   
	   //Id da variavel
	   process(&prox, &rec);
	   buf[4] = atoi(rec);
	   
	   //Valor
	   process(&prox, &rec);
	   tensao = atof(rec);
	   conteudo = (unsigned int) ((tensao+10)*262143)/20.0;

	   for(i = atoi(tamanho)+4; i > 4; i--)
	   {		   
		   buf[i] = conteudo & 255;		   
		   conteudo = conteudo >> 8;		
	   }
	   
	    i = 0;	    
	   	while(i < atoi(tamanho)+5)
		{						
			checksum += buf[i]&255;
			i++;
		}
	   
	   buf[atoi(tamanho)+5] = ~(checksum & 255) + 1;
	   	
	   return result;
}

void Comando :: lerVariavel()
{
		char * rec;
	    unsigned int checksum = 0;
	    int indice = 0;
	    buf = (char *) malloc(6*sizeof(char));	    
	    
		//Destino
		process(&prox, &rec);
		
		buf[0] = atoi(rec);		
		buf[1] = 0;
		buf[2] = LER_VARIAVEL;
		buf[3] = 1;
		
		//Id variavel
		process(&prox, &rec);
		debug(rec);
					
		buf[4] = atoi(rec);		
		
		while(indice <= 4)
		{
			checksum += buf[indice];
			indice++;			
		}
		
		buf[5] = ~(checksum & 0xFF) + 1;	
}

int Comando :: transmitirBlocoCurva()
{
	char * rec;
	buf = (char *) malloc(6*sizeof(char));
	
	//Destino
	process(&prox, &rec);
	
	buf[0] = atoi(rec);
	buf[1] = 0;
	buf[2] = TRANSMITIR_BLOCO_CURVA;
	buf[3] = 2;
	
	//Id curva
	process(&prox, &rec);	
	buf[4] = atoi(rec);
	
	//Offset do bloco
		
	process(&prox, &rec);	
	buf[5] = atoi(rec);
	
	return 6; //2 do enderecamento, 2 do cabecalho e 2 da carga
}
