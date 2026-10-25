#include "image_saver.h"

#include <SDL3_image/SDL_image.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define ARQUIVO_EXISTE(p) (_access((p), 0) == 0)
#else
#include <unistd.h>
#define ARQUIVO_EXISTE(p) (access((p), F_OK) == 0)
#endif

bool salvar_imagem(SDL_Surface *imagem, const char *caminho) {
    if (imagem == NULL || caminho == NULL) {
        fprintf(stderr, "Erro: nada para salvar.\n");
        return false;
    }

    const bool ja_existia = ARQUIVO_EXISTE(caminho);

    if (!IMG_SavePNG(imagem, caminho)) {
        fprintf(
            stderr,
            "Erro ao salvar a imagem: %s\n",
            SDL_GetError()
        );

        return false;
    }

    printf(
        "Arquivo '%s' %s.\n",
        caminho,
        ja_existia ? "sobrescrito" : "criado"
    );

    return true;
}
