/*
 * OPERATING SYSTEMS DESING - 16/17
 *
 * @file 	filesystem.c
 * @brief 	Implementation of the core file system funcionalities and auxiliary functions.
 * @date	01/03/2017
 */

#include "include/filesystem.h" // Headers for the core functionality
#include "include/auxiliary.h"  // Headers for auxiliary functions
#include "include/metadata.h"   // Type and structure declaration of the file system
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h> 

/*
 * @brief 	Generates the proper file system structure in a storage device, as designed by the student.
 * @return 	0 if success, -1 otherwise.
 */

TipoSuperbloque sbloques[1];
TipoInodoDisco inodos[MAX_FILES];
struct {
	int puntero;
	int abierto;
} inodos_x[MAX_FILES];

int totalElementos = 0;


int mkFS(long deviceSize){

	if (deviceSize < 51200 || deviceSize > 102400){
    	return -1;
    }
	/*inicializar a los valores por defecto del superbloque, mapas e inodos*/
	sbloques[0].numMagico = 1234;
	sbloques[0].numInodos = MAX_FILES;
	sbloques[0].numBloquesDatos = MAX_FILES;
	sbloques[0].tamDispositivo = deviceSize;
	sbloques[0].primerBloqueDatos = 2;
	totalElementos = 0;

	for(int i=0; i<sbloques[0].numInodos; i++){
		i_map[i]=0; /*inodo libre*/
	}

	for(int i=0; i<sbloques[0].numBloquesDatos; i++){
		b_map[i]=0; /*bloque libre*/
	}

	for(int i=0; i<sbloques[0].numInodos; i++){
		memset(&(inodos[i]), 0, sizeof(TipoInodoDisco));
	}

	for(int i=0; i<MAX_FILES; i++){
		for(int j=0; j<MAX_LOCAL_FILES; j++){
			inodos[i].inodosContenido[j] = -1; 
		}
	}

	/*escribir los valores por defecto al disco*/
	syncAux();
	return 0;
}

/*
 * @brief 	Mounts a file system in the simulated device.
 * @return 	0 if success, -1 otherwise.
 */
int mountFS(void){
	/*leer bloque 1 de disco en sbloques[0]*/
	if(bread(DEVICE_IMAGE, 1, ((char *)&(sbloques[0]))) == -1){
		return -1;
	}

	/*leer los bloques para el mapa de inodos*/
	for(int i=0; i<sbloques[0].numBloquesMapaInodos; i++){
		if(bread(DEVICE_IMAGE, 2+i, ((char *)i_map + i*BLOCK_SIZE)) == -1){
			return -1;
		}
	}

	/*leer los bloques para el mapa de bloques de datos*/
	for(int i=0; i<sbloques[0].numBloquesMapaDatos; i++){
		if(bread(DEVICE_IMAGE, 2+i+sbloques[0].numBloquesMapaInodos, ((char *)b_map + i*BLOCK_SIZE)) == -1){
			return -1;
		}
	}

	/*leer los inodos a memoria*/
	for(int i=0; i<(sbloques[0].numInodos*sizeof(TipoInodoDisco)/BLOCK_SIZE);i++){
		if(bread(DEVICE_IMAGE, i+sbloques[0].primerInodo, ((char *)inodos + i*BLOCK_SIZE)) == -1){
			return -1;
		}
	}

	return 0;
}

/*
 * @brief 	Unmounts the file system from the simulated device.
 * @return 	0 if success, -1 otherwise.
 */
int unmountFS(void){


	/*asegurarse de que todos los fichero están cerrados*/
	for(int i=0; i<sbloques[0].numInodos; i++){
		if(inodos_x[i].abierto == 1){
			return -1;
		}
	}

	/*escribir a disco los metadatos*/
	if(syncAux() == -1){
		return -1;
	}

	return 0;
}

/*
 * @brief	Creates a new file, provided it it doesn't exist in the file system.
 * @return	0 if success, -1 if the file already exists, -2 in case of error.
 */
int createFile(char *path){

	if(totalElementos >= 40){
		printf("ERROR: Ya no caben más elementos.\n");
		return -2;
	}

	/*COMPROBAR QUE SOLO TIENE 3 NIVELES Y SI TIENE MAS DEVOLVER -2*/
	char *token;
	char aux[132];
	char nombre[4][50];
	strcpy(aux, path);
	int cont = 0;
	token = strtok(aux, "/");
	while(token != NULL) {
		if(cont < 4){
			strcpy(nombre[cont], token);
			/*comprobar que el nombre del fichero no es superior a 32 bytes*/
		  	if(strlen(nombre[cont]) > 32){
		  		printf("ERROR: El fichero tiene más de 32 bytes.\n");
				return -2;
		  	}
			cont++;
    		token = strtok(NULL, "/");
		}else{
			printf("ERROR: Máximo 4 niveles incluido el fichero.\n");
  			return -2;
		}
  	}

  	if(cont == 2){
  		int fd = namei(nombre[0]);
  		if(fd < 0){
  			printf("ERROR: No se puede crear el fichero.\n");
  			return -2;
  		}
  		if(inodos[fd].tipo == T_FICHERO){
  			printf("ERROR: No se puede crear un fichero dentro de otro fichero.\n");
  			return -2;
  		}
  	}else if(cont == 3){
  		char str[132];
		strcpy(str, nombre[0]);
		strcat(str, "/");
		strcat(str, nombre[1]);
  		int fd = namei(str);
  		if(fd < 0){
  			printf("ERROR: No se puede crear el fichero.\n");
  			return -2;
  		}
  		if(inodos[fd].tipo == T_FICHERO){
  			printf("ERROR: No se puede crear un fichero dentro de otro fichero.\n");
  			return -2;
  		}
  	}else if(cont == 4){
  		char str[132];
		strcpy(str, nombre[0]);
		strcat(str, "/");
		strcat(str, nombre[1]);
		strcat(str, "/");
		strcat(str, nombre[2]);
  		int fd = namei(str);
  		if(fd < 0){
  			printf("ERROR: No se puede crear el fichero.\n");
  			return -2;
  		}
  		if(inodos[fd].tipo == T_FICHERO){
  			printf("ERROR: No se puede crear un fichero dentro de otro fichero.\n");
  			return -2;
  		}
  	}

	int b_id, inodo_id;

	/*COMPROBAR QUE NO ESTA REPETIDO Y SI LO ESTA DEVOLVER -1*/
	inodo_id = namei(path);
	if(inodo_id >= 0){
		printf("ERROR: El fichero ya existe.\n");
		return -1;
	}

	inodo_id = ialloc();
	if(inodo_id < 0){
		return -2;
	}
	b_id = alloc();
	if(b_id < 0){
		ifreeAux(inodo_id);
		return -2;
	}

	inodos[inodo_id].tipo = T_FICHERO; /*FICHERO*/
	strcpy(inodos[inodo_id].nombre, path);
	inodos[inodo_id].bloqueDirecto = b_id;
	inodos_x[inodo_id].puntero = 0;
	inodos_x[inodo_id].abierto = 0;
	totalElementos++;
	return 0;
}

/*
 * @brief	Deletes a file, provided it exists in the file system.
 * @return	0 if success, -1 if the file does not exist, -2 in case of error..
 */
int removeFile(char *path){

	if(totalElementos <= 0){
		printf("ERROR: No hay elementos.\n");
		return -2;
	}

	/*COMPROBAR QUE SOLO TIENE 3 NIVELES Y SI TIENE MAS DEVOLVER -2*/
	char *token;
	char aux[132];
	char nombre[50];
	strcpy(aux, path);
	int cont = 0;
	token = strtok(aux, "/");
	while(token != NULL) {
		if(cont < 4){
			strcpy(nombre, token);
			cont++;
    		token = strtok(NULL, "/");
		}else{
			printf("ERROR: Máximo 3 niveles.\n");
  			return -2;
		}
  	}

  	/*comprobar que el nombre del fichero no es superior a 32 bytes*/
  	if(strlen(nombre) > 32){
  		printf("ERROR: El fichero tiene más de 32 bytes.\n");
		return -2;
  	}

	int inodo_id;

	inodo_id = namei(path);
	if(inodo_id < 0){
		printf("ERROR: El fichero no existe.\n");
		return -1;
	}
	freeAux(inodos[inodo_id].bloqueDirecto);
	memset(&(inodos[inodo_id]), 0, sizeof(TipoInodoDisco));
	ifreeAux(inodo_id);
	totalElementos--;
	return 0;
}

/*
 * @brief	Opens an existing file.
 * @return	The file descriptor if possible, -1 if file does not exist, -2 in case of error..
 */
int openFile(char *path){

	/*COMPROBAR QUE SOLO TIENE 3 NIVELES Y SI TIENE MAS DEVOLVER -2*/
	char *token;
	char aux[132];
	char nombre[50];
	strcpy(aux, path);
	int cont = 0;
	token = strtok(aux, "/");
	while(token != NULL) {
		if(cont < 4){
			strcpy(nombre, token);
			cont++;
    		token = strtok(NULL, "/");
		}else{
			printf("ERROR: Máximo 3 niveles.\n");
  			return -2;
		}
  	}

  	/*comprobar que el nombre del fichero no es superior a 32 bytes*/
  	if(strlen(nombre) > 32){
  		printf("ERROR: El fichero tiene más de 32 bytes.\n");
		return -2;
  	}


	int inodo_id;
	/*buscar el inodo asociado al nombre*/
	char auxAux[132];
	strcpy(auxAux, path);
	inodo_id = namei(path);
	if(inodo_id < 0){
		return -1;
	}
	/*controlo que no esté ya abierto*/
	if(inodos_x[inodo_id].abierto == 1){
		return -2;
	}
	/*iniciar sesión de trabajo*/
	inodos_x[inodo_id].puntero = 0;
	inodos_x[inodo_id].abierto = 1;
	
	return inodo_id;
}

/*
 * @brief	Closes a file.
 * @return	0 if success, -1 otherwise.
 */
int closeFile(int fileDescriptor){
	/*comprobar descriptor válido*/
	if((fileDescriptor < 0) || (fileDescriptor > sbloques[0].numInodos - 1)){
		return -1;
	}
	/*Controlar que el fichero este abierto*/
	if(inodos_x[fileDescriptor].abierto == 1){
		/*cerrar sesión de trabajo*/
		inodos_x[fileDescriptor].puntero = 0;
		inodos_x[fileDescriptor].abierto = 0;
		return 0;
	}else{
		printf("ERROR: No existe o está cerrado.");
		return -1;
	}
}

/*
 * @brief	Reads a number of bytes from a file and stores them in a buffer.
 * @return	Number of bytes properly read, -1 in case of error.
 */
int readFile(int fileDescriptor, void *buffer, int numBytes){

	char b[BLOCK_SIZE];
	int b_id;

	if(inodos_x[fileDescriptor].abierto == CERRADO){
		printf("ERROR: El fichero está cerrado.\n");
		return -1;
	}

	if(inodos_x[fileDescriptor].puntero+numBytes > inodos[fileDescriptor].tamanyo){
		numBytes = inodos[fileDescriptor].tamanyo - inodos_x[fileDescriptor].puntero;
	}

	if(numBytes <= 0){
		return 0;
	}

	b_id = bmap(fileDescriptor, inodos_x[fileDescriptor].puntero);
	if(bread(DEVICE_IMAGE, b_id, b) == -1){
		return -1;
	}	
	memmove(buffer, b+inodos_x[fileDescriptor].puntero, numBytes);
	inodos_x[fileDescriptor].puntero += numBytes;

	return numBytes;
}

/*
 * @brief	Writes a number of bytes from a buffer and into a file.
 * @return	Number of bytes properly written, -1 in case of error.
 */
int writeFile(int fileDescriptor, void *buffer, int numBytes){
	char b[BLOCK_SIZE];
	int b_id;

	if(inodos_x[fileDescriptor].abierto == CERRADO){
		printf("ERROR: El fichero está cerrado.\n");
		return -1;
	}

	if(inodos_x[fileDescriptor].puntero+numBytes > BLOCK_SIZE){
		numBytes = BLOCK_SIZE - inodos_x[fileDescriptor].puntero;
	}

	if(numBytes <= 0){
		return 0;
	}

	b_id = bmap(fileDescriptor, inodos_x[fileDescriptor].puntero);
	if(bread(DEVICE_IMAGE, b_id, b) == -1){
		return -1;
	}
	memmove(b+inodos_x[fileDescriptor].puntero, buffer, numBytes);
	if(bwrite(DEVICE_IMAGE, b_id, b) == -1){
		return -1;
	}
	inodos_x[fileDescriptor].puntero += numBytes;
	inodos[fileDescriptor].tamanyo += numBytes;
  	return numBytes;
}

/*
 * @brief	Modifies the position of the seek pointer of a file.
 * @return	0 if succes, -1 otherwise.
 */
int lseekFile(int fileDescriptor, long offset, int whence){

	if(inodos_x[fileDescriptor].abierto == CERRADO){
		printf("ERROR: El fichero está cerrado.\n");
		return -1;
	}

	if(fileDescriptor < 0 || fileDescriptor > MAX_FILES-1){
    	return -1;
    }
    if (whence == FS_SEEK_CUR &&
      (((inodos_x[fileDescriptor].puntero + offset) > (BLOCK_SIZE - 1)) || ((inodos_x[fileDescriptor].puntero + offset) < 0))){
    	return -1;
 	}
  	if(whence == FS_SEEK_CUR){
  		inodos_x[fileDescriptor].puntero += offset;
	}else if(whence == FS_SEEK_BEGIN){
		inodos_x[fileDescriptor].puntero = 0;
	}else if(whence == FS_SEEK_END){
		inodos_x[fileDescriptor].puntero = inodos[fileDescriptor].tamanyo;
	}
	return 0;
}

/*
 * @brief	Creates a new directory provided it it doesn't exist in the file system.
 * @return	0 if success, -1 if the directory already exists, -2 in case of error.
 */
int mkDir(char *path){

	if(totalElementos >= 40){
		printf("ERROR: Ya no caben más elementos.\n");
		return -2;
	}

	/*COMPROBAR QUE SOLO TIENE 3 NIVELES Y SI TIENE MAS DEVOLVER -2*/
	char *token;
	char aux[99];
	char nombre[3][50];
	strcpy(aux, path);
	int cont = 0;
	token = strtok(aux, "/");
	while(token != NULL) {
		if(cont < 3){
			strcpy(nombre[cont], token);
			/*comprobar que el nombre del directorio no es superior a 32 bytes*/
		  	if(strlen(nombre[cont]) > 32){
		  		printf("ERROR: El directorio tiene más de 32 bytes.\n");
				return -2;
		  	}
			cont++;
    		token = strtok(NULL, "/");
		}else{
			printf("ERROR: Máximo 3 niveles.\n");
  			return -2;
		}
  	}

  	if(cont == 2){
  		int fd = namei(nombre[0]);
  		if(fd < 0){
  			printf("ERROR: No se puede crear el directorio.\n");
  			return -2;
  		}
  		if(inodos[fd].tipo == T_FICHERO){
  			printf("ERROR: No se puede crear un directorio dentro de un fichero.\n");
  			return -2;
  		}
  	}else if(cont == 3){
  		char str[99];
		strcpy(str, nombre[0]);
		strcat(str, "/");
		strcat(str, nombre[1]);
  		int fd = namei(str);
  		if(fd < 0){
  			printf("ERROR: No se puede crear el directorio.\n");
  			return -2;
  		}
  		if(inodos[fd].tipo == T_FICHERO){
  			printf("ERROR: No se puede crear un directorio dentro de un fichero.\n");
  			return -2;
  		}
  	}

	int b_id, inodo_id;

	/*COMPROBAR QUE NO ESTA REPETIDO Y SI LO ESTA DEVOLVER -1*/
	inodo_id = namei(path);
	if(inodo_id >= 0){
		printf("ERROR: El directorio ya existe.\n");
		return -1;
	}

	inodo_id = ialloc();
	if(inodo_id < 0){
		return -2;
	}
	b_id = alloc();
	if(b_id < 0){
		ifreeAux(inodo_id);
		return -2;
	}

	if(inodos[cont-1].numElementos == MAX_LOCAL_FILES){
		return -2; /*No se pueden crear directorios porque el directorio está lleno*/
	}

	int boolean = 0;
	for(int i=0; i<MAX_LOCAL_FILES; i++){
		if(boolean == 0){
			if(inodos[cont-1].inodosContenido[i] == -1){
			boolean = 1;
			inodos[cont-1].inodosContenido[i] = inodo_id;
			inodos[cont-1].numElementos = inodos[cont-1].numElementos + 1;
			}
		}
	}

	inodos[inodo_id].tipo = T_DIRECTORIO; /*DIRECTORIO*/
	strcpy(inodos[inodo_id].nombre, path);
	inodos[inodo_id].bloqueDirecto = b_id;
	inodos_x[inodo_id].abierto = 0;
	inodos[inodo_id].numElementos = 0;
	totalElementos++;
	for(int i=0; i<MAX_LOCAL_FILES; i++){
		inodos[inodo_id].inodosContenido[i] = -1;
	}
	
	return 0;
}

/*
 * @brief	Deletes a directory, provided it exists in the file system.
 * @return	0 if success, -1 if the directory does not exist, -2 in case of error..
 */
int rmDir(char *path){

	if(totalElementos <= 0){
		return -2;
	}

	int inodo_id;

	inodo_id = namei(path);
	if(inodo_id < 0){
		return -1;
	}
	for(int i=0; i<inodos[inodo_id].numElementos; i++){
		ifreeAux(inodos[inodo_id].inodosContenido[i]);
		totalElementos--;
	}

	freeAux(inodos[inodo_id].bloqueDirecto);
	memset(&(inodos[inodo_id]), 0, sizeof(TipoInodoDisco));
	ifreeAux(inodo_id);
	totalElementos--;

	return 0;
}

/*
 * @brief	Lists the content of a directory and stores the inodes and names in arrays.
 * @return	The number of items in the directory, -1 if the directory does not exist, -2 in case of error..
 */
int lsDir(char *path, int inodesDir[10], char namesDir[10][33]){
	/*COMPROBAR QUE SOLO TIENE 3 NIVELES Y SI TIENE MAS DEVOLVER -2*/
	char *token;
	char aux[99];
	char nombre[3][50];
	strcpy(aux, path);
	int cont = 0;
	token = strtok(aux, "/");
	while(token != NULL) {
		if(cont < 3){
			strcpy(nombre[cont], token);
			/*comprobar que el nombre del directorio no es superior a 32 bytes*/
		  	if(strlen(nombre[cont]) > 32){
		  		printf("ERROR: El directorio tiene más de 32 bytes.\n");
				return -2;
		  	}
			cont++;
    		token = strtok(NULL, "/");
		}else{
			printf("ERROR: Máximo 3 niveles.\n");
  			return -2;
		}
  	}

  	int inodo_id;

	/*COMPROBAR QUE EXISTE EL DIRECTORIO*/
	inodo_id = namei(path);
	if(inodo_id < 0){
		printf("ERROR: El directorio no existe.\n");
		return -1;
	}

	for(int i=0; i<MAX_LOCAL_FILES; i++){
		inodesDir[i] = inodos[inodo_id].inodosContenido[i];
		strcpy(namesDir[i], inodos[inodos[inodo_id].inodosContenido[i]].nombre);
	}

	return 2;

}


/*FUNCIONES AUXILIARES*/

/*FUNCION 1*/
int syncAux (void){
	/*escribir bloque 1 de sbloques[0] a disco*/
	if(bwrite(DEVICE_IMAGE, 1, ((char *)&(sbloques[0]))) == -1){
		return -1;
	}

	/*escribir los bloques para el mapa de inodos*/
	for(int i=0; i<sbloques[0].numBloquesMapaInodos; i++){
		if(bwrite(DEVICE_IMAGE, 2+i, ((char *)i_map + i*BLOCK_SIZE)) == -1){
			return -1;
		}
	}

	/*escribir los bloques para el mapa de bloques de datos*/
	for(int i=0; i<sbloques[0].numBloquesMapaDatos; i++){
		if(bwrite(DEVICE_IMAGE, 2+i+sbloques[0].numBloquesMapaInodos, ((char *)b_map + i*BLOCK_SIZE)) == -1){
			return -1;
		}
	}

	/*escribir los inodos a disco*/
	for(int i=0; i<(sbloques[0].numInodos*sizeof(TipoInodoDisco)/BLOCK_SIZE);i++){
		if(bwrite(DEVICE_IMAGE, i+sbloques[0].primerInodo, ((char *)inodos + i*BLOCK_SIZE)) == -1){
			return -1;
		}
	}
	return 0;
}

/*FUNCION 2*/
int ialloc(void){
	/*buscar un inodo libre*/
	for(int i=0; i<sbloques[0].numInodos; i++){
		if(i_map[i] == 0){
			/*inodo ocupado ahora*/
			i_map[i] = 1;
			/*valores por defecto en el inodo*/
			memset(&(inodos[i]), 0, sizeof(TipoInodoDisco));
			/*devolver identificador de inodo*/
			//printf("Identificador\n");
			return i;
		}
	}
	//printf("Menos 1\n");
	return -1;
}

/*FUNCION 3*/
int alloc(void){
	char b[BLOCK_SIZE];
	for(int i=0; i<sbloques[0].numBloquesDatos; i++){
		if(b_map[i]==0){
			/*bloque ocupado ahora*/
			b_map[i]=1;
			/*valores por defecto en el bloque*/
			memset(b,0,BLOCK_SIZE);
			bwrite(DEVICE_IMAGE, i+sbloques[0].primerInodo, b);
			/*devolver identificador del bloque*/
			return i;
		}
	}
	return -1;
}

/*FUNCION 4*/
int ifreeAux(int inodo_id){
	/*comprobar validez de inodo_id*/
	if(inodo_id > sbloques[0].numInodos){
		return -1;
	}
	/*liberar inodo*/
	i_map[inodo_id]=0;
	return 0;
}

/*FUNCION 5*/
int freeAux(int block_id){
	/*comprobar validez de block_id*/
	if(block_id > sbloques[0].numBloquesDatos){
		return -1;
	}
	/*liberar bloque*/
	b_map[block_id]=0;
	return 0;
}

/*FUNCION 6*/
int namei(char *fname){
	char aux[132];
	strcpy(aux, fname);
	//printf("FNAME: %s\n", aux);
	/*buscar inodo con nombre <fname>*/
	for(int i=0; i<MAX_FILES; i++){
		//printf("INODOS NOMBRE: %s\n", inodos[i].nombre);
		if(!strcmp(inodos[i].nombre, fname)){
			//printf("NUMERO DE INODO: %d", i);
			return i;
		}
	}
	//printf("NAMEI DEVUELVE -1\n");
	return -1;
}

/*FUNCION 7*/
int bmap(int inodo_id, int offset){
	/*comprobar validez de inodo_id*/
	if(inodo_id > sbloques[0].numInodos){
		return -1;
	}
	/*bloque de datos asociado*/
	if(offset < BLOCK_SIZE){
		return inodos[inodo_id].bloqueDirecto;
	}
	return -1;
}