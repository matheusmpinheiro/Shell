/*
* RSFS - Really Simple File System
*
* Copyright © 2010 Gustavo Maciel Dias Vieira
* Copyright © 2010 Rodrigo Rocco Barbieri
*
* This file is part of RSFS.
*
* RSFS is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

//====================[ Configurações ]====================

#define CLUSTERSIZE 4096

//====================[ Menssagens de Erro ]====================

#define ARQ_FECHADO  			"[Erro] O arquivo esta fechado\n"
#define ARQ_JA_EXISTE 			"[Erro] O arquivo já existe!\n"
#define ARQ_INVALIDO 			"[Erro] Identificador de arquivo invalido\n"
#define ARQ_NAO_ABERTO_PARA 	"[Erro] O arquivo nao esta aberto para a %s\n"
#define ARQ_NAO_CARREGADO 		"[Erro] Nao foi possivel carregar o arquivo\n"
#define ARQ_NAO_CRIADO 			"[Erro] Nao foi possivel criar o novo arquivo\n"
#define ARQ_NAO_ENCONTRADO 		"[Erro] Nao foi possivel achar o arquivo\n"
#define ARQ_NAO_REMOVIDO 		"[Erro] Nao foi possivel remover o arquivo\n"
#define ARQ_MODO_NAO_SUPORTADO  "[Erro] Modo de arquivo não suportado\n"
#define ARQ_TAM_NOME 			"[Erro] O nome do arquivo deve ter no máximo 24 chars!\n"

#define BUF_NAO_ALOCADO 		"[Erro] Nao foi possivel alocar o buffer do arquivo\n"
#define BUF_NAO_SALVO   		"[Erro] Nao foi possivel salvar o buffer do arquivo\n"

#define DIR_CHEIO 				"[Erro] O diretorio está cheio\n"
#define DIR_NAO_ATUALIZADO  	"[Erro] Nao foi possivel atualizar o diretorio!\n"

#define FAT_CORROMPIDA 			"\x1b[0G\x1b[0K[Erro] FAT corrompida!(entrada %d, valor = %hu )\n"
#define FAT_INVALIDA 			"[Erro] A FAT esta inconsistente(corrompida ou nao formatada)\n"
#define FAT_OK 					"\x1b[0G\x1b[0KFAT esta OK\n"
#define FAT_VERIFICANDO 		"\x1b[0GVerificando FAT: %f %%"
#define FAT_NAO_ATUALIZADA  	"[Erro] Nao foi possivel atualizar a FAT!\n"

#define HD_CHEIO 				"[Erro] O hd esta cheio\n"

#define SET_NAO_ESCRITO 		"[Erro] Nao foi possivel escrever o setor %d\n"
#define SET_NAO_LIDO    		"[Erro] Nao foi possivel ler o setor %d\n"

//====================[ Definições das Estruturas de Dados ]====================

typedef struct {
	char used;
	char name[25];
	unsigned short first_block;
	int size;
} dir_entry;

typedef struct {
	char opened;
	char permition;
	int block;
	int byte;
	char* buffer;
	int size;
	int dir_pos;
} opened_file;

//====================[ Variaveis Globais ]====================

dir_entry 		dir[128];
unsigned short  fat[65536];
char 			FATValida;
opened_file 	of[128];

//====================[ Definições de Funções Uteis]====================

int atualizar_particao();
int carrega_arq(int dir_pos,int file_pos,int mode);
int escrever_blocos(char* buffer, int pos, int qtd);
int ler_blocos(char* buffer, int pos, int qtd);


//====================[ Código ]====================

// salva no disco a tabela FAT e o diretório
int atualizar_particao(){
	if(!escrever_blocos((char*) fat,0,32 ) ){
		printf(FAT_NAO_ATUALIZADA);
		return  0;
	}

	if(!escrever_blocos((char*) dir,32,1 ) ){
		printf(DIR_NAO_ATUALIZADO);
		return  0;
	}

	return 1;
}

// carrega um arquivo para a memória, ou seja aloca um
// identificador para o arquivo, coloca na memória alguns
// atributos do arquivo e inicializa o buffer do arquivo
int carrega_arq(int dir_pos,int file_pos,int mode){
	of[file_pos].opened = 1;
	of[file_pos].permition = mode;
	of[file_pos].block = dir[dir_pos].first_block;
	of[file_pos].byte = 0;
	of[file_pos].size = dir[dir_pos].size;
	of[file_pos].dir_pos = dir_pos;
	of[file_pos].buffer = (char*)malloc(sizeof(char)*CLUSTERSIZE);
	if(of[file_pos].buffer == NULL){
		of[file_pos].opened = 0;
		printf(BUF_NAO_ALOCADO);
		return -1;
	}
	if(!ler_blocos(of[file_pos].buffer,of[file_pos].block,1)){
		of[file_pos].opened = 0;
		return -1;
	}
	return 0;
}


//le a patir de pos qtd blocos para o buffer
// em caso de erro retorna 0
int ler_blocos(char* buffer, int pos, int qtd){
	int i;
	pos *= 8;
	qtd *= 8;
	for(i = 0; i <  qtd; i++){
		if(!bl_read(i+pos,&buffer[i*SECTORSIZE])){
			printf(SET_NAO_LIDO, i+pos);
			return 0;
		}
	}
	return 1;
}

// escreve no disco o buffer usando qtd 
// de blocos apartir do setor pos
int escrever_blocos(char* buffer, int pos, int qtd){
	int i;
	pos *= 8;
	qtd *= 8;
	for(i = 0; i <  qtd; i++){
		if(!bl_write(i+pos,&buffer[i*SECTORSIZE])){
			printf(SET_NAO_ESCRITO, i+pos);
			return 0;
		}
	}
	return 1;	
}

int fs_init() {
	char* fat_ = (char *) fat;
	char* dir_ = (char *) dir;
	int i;

	for(i = 0; i < 128; i++){
		of[i].opened = 0;
	}

	if(!ler_blocos(fat_,0,32)){
		return 0;
	}

	if(!ler_blocos(dir_,32,1)){
		return 0;
	}

	for(i=0;i<65536;i++){
		if(i % 1000 == 0){
			printf(FAT_VERIFICANDO,(float)i / 65535.0 * 100.0);
		}
		if(i < 32 && fat[i] != 3){
			printf(FAT_CORROMPIDA,i,fat[i]);
			FATValida = 0;
			return 1;
		}
		if(i == 32 && fat[i]!=4){
			printf(FAT_CORROMPIDA,i,fat[i]);
			FATValida = 0;
			return 1;
		}
		if(i > 32 && (fat[i] <=32 && fat[i] != 1 && fat[i] != 2 ) ){
			printf(FAT_CORROMPIDA,i,fat[i]);
			FATValida = 0;
			return 1;
		}
	}
	printf(FAT_OK);
	FATValida = 1;

	return 1;
}

int fs_format() {
	int i;

// Inicializar as 32 posições iniciais da FAT
	for (i=0; i<32; i++){
		fat[i] = 3;
	}

// Inicializar posição referente ao diretório
	fat[32] = 4;

// Inicializar demais posições como posições livres
	for (i=33 ; i<65536; i++){
		fat[i] = 1;
	}

// Inicializar o diretório
	for (i=0; i<128; i++){
		dir[i].used = 0;
	}

	for(i = 0; i < 128; i++){
		of[i].opened = 0;
	}

	FATValida = atualizar_particao();
	return FATValida;
}

int fs_free() {
	int acum = 0, i;

	if(!FATValida){
		printf(FAT_INVALIDA);
		return 0;
	}

	for (i=33; i< bl_size()/8; i++){
		if (fat[i] == 1)
			acum++;
	}

	return acum * CLUSTERSIZE;
}

int fs_list(char *buffer, int size) {
	int i = 0,j = 0;
	char concat[1000];

	if(FATValida == 1){
		for(i = 0; i < 128; i++){
			if(dir[i].used == 1){
				dir[i].name[24] = '\0';
				sprintf(concat,"%s\t\t%d\n",dir[i].name,dir[i].size);
				if(j + strlen(concat) < size){;
					strncpy(&buffer[j], concat, 32);
					j += strlen(concat);
				}else{
					return 0;
				}
			}
		}

		return 1;
	}else{
		printf(FAT_INVALIDA);
		return 0;
	}

}

int fs_create(char* file_name) {
	int i;

	if(!FATValida){
		printf(FAT_INVALIDA);
		return 0;
	}

	if(strlen(file_name) > 24){
		printf(ARQ_TAM_NOME);
		return 0;
	}

	int dirPos = -1;

	for(i = 0; i < 128; i++){
// printf("%d\n",dir[i].used );
		if(dir[i].used == 0){
			if(dirPos == -1){
				dirPos = i;
			}
		}else{
			if(strcmp(dir[i].name,file_name) == 0){
				printf(ARQ_JA_EXISTE);
				return 0;
			}
		}
	}
	if(i == 128 && dirPos == -1){
		printf(DIR_CHEIO);
		return 0;
	}

	int fatPos = -1;

	for(i=33; i < 65536; i++){
		if(fat[i] == 1){
			fatPos = i;
			break;
		}
	}

	if(fatPos == -1){
		printf(HD_CHEIO);
		return 0;
	}

	if(fatPos > bl_size()){
		printf(HD_CHEIO);
		return 0;
	}

	fat[fatPos]=2;

	dir[dirPos].used = 1;
	strcpy(dir[dirPos].name,file_name);
	dir[dirPos].first_block = fatPos;
	dir[dirPos].size = 0;

	return atualizar_particao();
}

int fs_remove(char *file_name) {
	int i, pos_fat, pos_prox;

// Comparar as entradas do diretório com o nome do arquivo de entrada
	for (i=0; i<128; i++){
		if (strcmp(file_name, dir[i].name) == 0 && dir[i].used == 1)
			break;
	}

// Caso encontre
	if (i < 128){
// Pegar posição do primeiro bloco
		pos_fat = dir[i].first_block;

// Enquanto não chegar no ultimo bloco
		while(fat[pos_fat] != 2){
			pos_prox = fat[pos_fat];
			fat[pos_fat] = 1;
			pos_fat = pos_prox;
		}

// Zerar último bloco
		fat[pos_fat] = 1;

// Zerar entrada do diretório
		dir[i].used = 0;

// printf ("Arquivo removido com sucesso!\n");
		return atualizar_particao();
	} else {
		printf (ARQ_NAO_ENCONTRADO);
		return 0;
	}
}

int fs_open(char *file_name, int mode) {
	int i;
	int dir_pos  = -1;

	if(!FATValida){
		printf(FAT_INVALIDA);
		return -1;
	}

	if(!(mode ==  FS_R || mode == FS_W)){
		printf(ARQ_MODO_NAO_SUPORTADO);
		return -1;
	}

	// tenta achar o arquivo no diretorio
	for(i = 0; i < 128; i++){
		if(!dir[i].used){
			if(dir_pos == -1){
				dir_pos = i;
			}
			continue;
		}
		if(strcmp(file_name,dir[i].name) == 0){
			// se estamos abrindo para escrita devemos apagar o arquivo
			if(mode == FS_W){
				if(!fs_remove(file_name)){
					printf(ARQ_NAO_REMOVIDO);
					return -1;
				}
			}
			break;
		}
	}

	// se nao achou o arquivo e estamos abrindo para leitura
	if(mode == FS_R && i == 128){
		printf(ARQ_NAO_ENCONTRADO);
		return -1;
	}

	// tenta achar um identificador vazio para carregar o arquivo
	int j;
	for(j = 0; j < 128; j++){

		if(!of[j].opened){
			// se estamos abrindo para leitura so carregamos o arquivo
			if(mode == FS_R){
				if(carrega_arq(i,j,mode) == -1){
					printf(ARQ_NAO_CARREGADO);
					return -1;
				}	
				break;
			}else{
				// se estamos abrindo para escrita cria o arquivo antes de carregar
				if( dir_pos == -1){
					dir_pos = i;
				}

				if(dir_pos > i){
					dir_pos = i;
				}
				if(!fs_create(file_name)){
					printf(ARQ_NAO_CRIADO);
					return -1;
				}  	
				if(carrega_arq(dir_pos,j,mode) == -1){
					printf(ARQ_NAO_CARREGADO);
					return -1;
				}		

				break;
			}	
		}
	}

	return j;
}

int fs_close(int file)  {
	if(!of[file].opened){
		printf(ARQ_FECHADO);
		return 0;
	}

	dir[of[file].dir_pos].size = of[file].size;

	if(!escrever_blocos(of[file].buffer,of[file].block,1)){
		printf(BUF_NAO_SALVO);
		return 0;
	}

	free(of[file].buffer);
	of[file].opened = 0;
	return atualizar_particao();
}

int fs_write(char *buffer, int size, int file) {
	if(file >= 128 || file < 0){
		printf(ARQ_INVALIDO);
		return -1;
	}

	if(!of[file].opened){
		printf(ARQ_FECHADO);
		return -1;
	}

	if(of[file].permition == FS_R){
		printf(ARQ_NAO_ABERTO_PARA,"escrita");
		return -1;
	}

	int i;
	for(i = 0; i < size; i++){

		if(of[file].byte == CLUSTERSIZE){
			if(!escrever_blocos(of[file].buffer,of[file].block,i)){
				return i;
			}
			int j;
			for(j = 33; j < 65536;j++){
				if(fat[j] == 1){
					fat[of[file].block] = j;
					fat[j] = 2;
					of[file].block = j;
					break;
				}
			}
			if( j == 65536 ){

				return i;
			}
			of[file].byte = 0;
		}
		of[file].buffer[of[file].byte++] = buffer[i];
		of[file].size++;
	}
	return i;

}

int fs_read(char *buffer, int size, int file) {
	opened_file* opf;

	if(file >= 128 || file < 0){
		printf(ARQ_INVALIDO);
		return -1;
	}

	if(!of[file].opened){
		printf(ARQ_FECHADO);
		return -1;
	}

	if(of[file].permition == FS_W){
		printf(ARQ_NAO_ABERTO_PARA,"leitura");
		return -1;
	}

	opf = &of[file];

	int i;
	for(i = 0; i < size; i++){
		if(fat[opf->block] == 2 && opf->byte >= (opf->size % CLUSTERSIZE) ){
			return i;
		}
		if(opf->byte  == CLUSTERSIZE){
			if(fat[opf->block] == 2){
				return i;
			}
			opf->block = fat[opf->block];
			if(!ler_blocos(opf->buffer,opf->block,1)){
				return -1;
			}
			opf->byte = 0;
		}
		buffer[i] = opf->buffer[opf->byte++];
	}
	return i;
}

