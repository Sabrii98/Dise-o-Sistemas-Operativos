/*
 * OPERATING SYSTEMS DESING - 16/17
 *
 * @file 	metadata.h
 * @brief 	Definition of the structures and data types of the file system.
 * @date	01/03/2017
 */

#define BLOCK_SIZE 2048
#define MAX_NAME_LENGHT 132
#define MAX_LOCAL_FILES 10
#define PADDING_SB 2016
#define T_FICHERO 1
#define T_DIRECTORIO 0
#define ABIERTO 1
#define CERRADO 0
#define PADDING_NODO 1964
#define MAX_FILES 40



typedef struct{
	unsigned int numMagico; /*Numero magico del superbloque*/
	unsigned int numBloquesMapaInodos; /*Numero de bloques del mapa inodos*/
	unsigned int numBloquesMapaDatos; /*Numero de bloques del mapa datos*/
	unsigned int numInodos; /*Numero de inodos en el dispositivo*/
	unsigned int primerInodo; /*Numero bloque del primer inodo del dispositivo (inodo raiz)*/
	unsigned int numBloquesDatos; /*Numero de bloques de datos en el dispositivo*/
	unsigned int primerBloqueDatos; /*Numero de bloques del primer bloque de datos*/
	unsigned int tamDispositivo; /*Tamanyo total del dispositivo (en bytes)*/
	char relleno[PADDING_SB]; /*Campo de relleno (para completar un bloque)*/
} TipoSuperbloque;

typedef struct{
	unsigned int tipo; /*T_FICHERO o T_DIRECTORIO*/
	char nombre[MAX_NAME_LENGHT]; /*Nombre del fichero/directorio asociado*/
	int inodosContenido[MAX_LOCAL_FILES]; /*tipo==dir: lista de los inodos del directorio*/
	unsigned int tamanyo; /*Tamanyo actual del fichero en bytes*/
	unsigned int bloqueDirecto; /*Numero del bloque directo*/
	unsigned int numElementos; /*Numero de elementos en un directorio*/  
	char relleno[PADDING_NODO]; /*Campo relleno para llenar un bloque*/
} TipoInodoDisco;

char i_map[MAX_FILES]; /*1 si el inodo esta en uso y 0 si esta libre*/
char b_map[MAX_FILES]; /*1 si el bloque esta en uso y 0 si esta libre*/


#define bitmap_getbit(bitmap_, i_) (bitmap_[i_ >> 3] & (1 << (i_ & 0x07)))
static inline void bitmap_setbit(char *bitmap_, int i_, int val_) {
  if (val_)
    bitmap_[(i_ >> 3)] |= (1 << (i_ & 0x07));
  else
    bitmap_[(i_ >> 3)] &= ~(1 << (i_ & 0x07));
}
