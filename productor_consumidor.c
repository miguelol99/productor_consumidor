//Miguel Antonio Nuñez Saiz: 71177611V

//Se emplean funciones de la biblioteca math.h por lo que al compilar
// hay que añadir el argumento -lm 

//Bibliotecas incluidas
#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdbool.h>
#include<ctype.h>
#include<math.h>
#include <semaphore.h>

//Constantes
#define NPARAM 4
#define MAX_NUM 3000000
#define LINE_SIZE 255

//Estructuras
typedef struct{
	int numDatos;
	int valorMinimo;
	int valorMaximo;
	long sumaTotal;
	double media;
}Vector;

typedef struct {
	int num_hilo;
	int rango_inferior;
	int rango_superior;
}Argumento;

//Buffer
int *buffer;

//Vector
Vector *vector;

//Variables globales
Argumento l;
int sig_llenar_buffer;
int sig_vaciar_buffer;
int tam_buffer;
int numConsumidores;

//Semaforos
sem_t mutex_svaciar_buffer;
sem_t hay_espacio_buffer;
sem_t hay_dato_buffer;
sem_t consumidor_terminado;
sem_t lector_disponible;


//Recibe un puntero a string y devuelve el string convertido a entero,
//o -1 si el string contiene caracteres no numéricos
int comprobarEntero(char *cadena) {
	int entero = atoi(cadena);

	//Para el caso en el que el entero sea 0
	if(entero == 0) {
		if(cadena[0] == '0' && strlen(cadena) == 1) 
			return entero;
		else 
			return -1;		
	}

	//Comprueba si la longitud del string es igual a la longitud del entero
	if(strlen(cadena) == floor(log10(entero)) + 1) 
		return entero;
	else 
		return -1;
}

//Hilo productor. Recibe como parametro un puntero a un string (nombre del fichero de entrada).
void *productor(void *arg){
	//Variables locales
	char *f = (char*) arg;
	char line[LINE_SIZE]; //(char*) malloc(LINE_SIZE * sizeof(char));
	int item;
	FILE *fichero = fopen(f, "r");
	
	while(fgets(line, LINE_SIZE, fichero)){

		//Eliminamos el caracter '\n' del string y lo convertimos a entero
		line[strlen(line)-1] = '\0';
		item = comprobarEntero(line);

		if(item >= 0 && item < 3000000) {
			
			sem_wait(&(hay_espacio_buffer));

			//Escribimos en el buffer y aumentamos sig_llenar_buffer
			buffer[sig_llenar_buffer] = item;		
	
			sig_llenar_buffer = (sig_llenar_buffer + 1) % tam_buffer;

			sem_post(&(hay_dato_buffer));
		}

	}

	sem_wait(&(hay_espacio_buffer));

	//Escribe en el buffer -1 (bandera)
	buffer[sig_llenar_buffer] = -1;

	//printf("Productor:\tB[%d]\tTERMINADO\n",sig_llenar_buffer);

	sem_post(&(hay_dato_buffer));

	fclose(fichero);

	//Termina el hilo
	pthread_exit(0);
}

//Hilo consumidor. Recibe como parametro un puntero a un int (numero del hilo)
void *consumidor(void *arg){	
	//Variables locales	
	Argumento *c = (Argumento*) arg;
	int indice = c->num_hilo;
	int inferior = c->rango_inferior;
	int superior = c->rango_superior;
	int item;

	while(1){
		sem_wait(&(hay_dato_buffer));

		sem_wait(&(mutex_svaciar_buffer));

		item = buffer[sig_vaciar_buffer];

		if(item == -1){ 
			
			sem_post(&(mutex_svaciar_buffer));

			sem_post(&(hay_dato_buffer));

			break;
		}
		
		else if(item < inferior || item > superior){	

			sem_post(&(mutex_svaciar_buffer));

			sem_post(&(hay_dato_buffer));
		}
		//Si el dato no se encuentra dentro de los limites establecidos
		else if(item >= inferior && item <= superior)	{
		
			sig_vaciar_buffer = (sig_vaciar_buffer + 1) % tam_buffer;

			sem_post(&(mutex_svaciar_buffer));

			sem_post(&(hay_espacio_buffer));
		
			//Se realizan los calculos pertinentes y se almacenan en la 
			//posicion del vector asignada
			vector[indice].numDatos = vector[indice].numDatos + 1;

			if(item > vector[indice].valorMaximo)
				vector[indice].valorMaximo = item;

			if(item < vector[indice].valorMinimo) 
				vector[indice].valorMinimo = item;

			vector[indice].sumaTotal = vector[indice].sumaTotal + item;
		}
	
	}
	vector[indice].media = vector[indice].sumaTotal / vector[indice].numDatos;

	sem_wait(&(lector_disponible));

	//Almacena en sig_vaciar_vector la posicion del vector para leer
	l.num_hilo = indice;
	l.rango_inferior = inferior;
	l.rango_superior = superior;	

	sem_post(&(consumidor_terminado));
	
	//Termina el hilo
	pthread_exit(0);
}

//Hilo lector. Recibe como parametro un puntero a un string (nombre del fichero de salida)
void *lector(void *arg){
	//Variables locales
	char *f = (char*) arg;
	int contador = 0;
	int indice;
	int inferior;
	int superior;
	FILE *fichero = fopen(f, "w");
	
	while(contador < numConsumidores) {

		//Espera a que haya terminado un consumidor
		sem_wait(&(consumidor_terminado));

		indice = l.num_hilo;
		inferior = l.rango_inferior;
		superior = l.rango_superior;
		
		//Escribe en el fichero de salida los datos del consumidor
		fprintf(fichero, "========= RANGO del Hilo %d [%d]-[%d]\n", indice, inferior, superior);
		fprintf(fichero,"Numero de Datos: %d\n",vector[indice].numDatos);
		fprintf(fichero,"Valor Minimo: %d\n",vector[indice].valorMinimo);
		fprintf(fichero,"Valor Maximo: %d\n",vector[indice].valorMaximo);
		fprintf(fichero,"Suma Total: %ld\n",vector[indice].sumaTotal);
		fprintf(fichero,"Media: %f\n\n",vector[indice].media);

		//Indica que el hilo lector esta disponible
		sem_post(&(lector_disponible));
		
		contador++;	
	}

	fclose(fichero);

	//Termina el hilo
	pthread_exit(0);
}


int main(int argc, char *argv[]){

	//Declarar variables locales
	char *inputFile;
	char *outputFile;
	int rango;
	pthread_t productorId;
	pthread_t *consumidorId;
	pthread_t lectorId;
	Argumento *c;
	
	//Comprobar los argumetos introducidos por el usuario
	if(argc != NPARAM + 1){
		printf("[ERROR] Se requiere la siguiente sintaxis:\n\n");
		printf("   ./<program> <inputFile> <outputFile> <tamBuffer> <numConsumidores>\n\n");
		fflush(stdout);
		return -1;
	}	
	if(fopen(argv[1], "r") == NULL){
		printf("[ERROR] Fichero de entrada no valido\n"); fflush(stdout);
		return -1; 
	}
	if(fopen(argv[2], "r") == NULL){
		printf("[ERROR] Fichero de salida no valido\n"); fflush(stdout);
		return -1; 
	}
	if(comprobarEntero(argv[3]) == -1){
		printf("[ERROR] Tamaño del buffer no valido\n"); fflush(stdout);
		return -1;
	}
	if(comprobarEntero(argv[4]) == -1){
		printf("[ERROR] Numero de consumidores no valido\n"); fflush(stdout);
		return -1;
	}

	//Incicializar variables globales
	sig_llenar_buffer = 0;
	sig_vaciar_buffer = 0;
	tam_buffer = comprobarEntero(argv[3]);
	numConsumidores = comprobarEntero(argv[4]);

	//Inicializar variables locales
	inputFile = argv[1];
	outputFile = argv[2];
	rango = (MAX_NUM / numConsumidores);

	//Reservar memoria
	buffer = (int*) malloc(numConsumidores * sizeof(int));
	vector = (Vector*) malloc(numConsumidores * sizeof(Vector));
	consumidorId = (pthread_t*) malloc(numConsumidores * sizeof(pthread_t));
	c = (Argumento*) malloc(numConsumidores * sizeof(Argumento));
	
	//Inicializar estructuras
	for(int i = 0; i < numConsumidores; i++){
		c[i].num_hilo = i;
		c[i].rango_inferior = rango * i;
		c[i].rango_superior = c[i].rango_inferior + (rango-1);
		if(i == (numConsumidores-1))
			c[i].rango_superior = MAX_NUM-1;
	}
	
	for(int i = 0; i < numConsumidores; i++){
		vector[i].numDatos = 0;
		vector[i].valorMinimo = 3000000;
		vector[i].valorMaximo = 0;
		vector[i].sumaTotal = 0;
		vector[i].media = 0;
	}

	//Inicializa los semaforos
	sem_init(&(hay_espacio_buffer), 0, tam_buffer);
	sem_init(&(hay_dato_buffer), 0, 0);
	sem_init(&(lector_disponible), 0, 1);
	sem_init(&(consumidor_terminado), 0, 0);
	sem_init(&(mutex_svaciar_buffer), 0, 1);
	
	//Crear hilos
	pthread_create(&productorId, NULL, productor, (void*) inputFile);
	for(int i = 0; i < numConsumidores; i++)
		pthread_create(&consumidorId[i], NULL, consumidor, (void*) &c[i]);
	pthread_create(&lectorId, NULL, lector, (void*) outputFile);

	//Esperar a los hilos
	pthread_join(productorId, NULL);	
	for(int i = 0; i < numConsumidores; i++)
		pthread_join(consumidorId[i], NULL);
	pthread_join(lectorId, NULL);

	
	//Destruir semaforos
	sem_destroy(&(hay_espacio_buffer));
	sem_destroy(&(hay_dato_buffer));
	sem_destroy(&(lector_disponible));
	sem_destroy(&(consumidor_terminado));
	sem_destroy(&(mutex_svaciar_buffer));	

	//Liberar memoria
	free(buffer);
	free(consumidorId);
	free(c);

	//Termina el programa
	return 0;
}
