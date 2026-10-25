#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <SDL3/SDL.h>

/*
 * Carrega uma imagem do disco e devolve a superfície já convertida para
 * RGBA32, formato usado por todas as etapas seguintes.
 *
 * Retorna NULL em caso de erro (arquivo inexistente, formato inválido ou
 * falha de conversão) e imprime o motivo no stderr.
 */
SDL_Surface *carregar_imagem(const char *caminho);

#endif
