#include "gui.h"
#include "gui_interno.h"

#include <stdio.h>

/*
 * Janela principal: exibe a imagem selecionada e desenha sobre ela a
 * marcação de cada QR Code encontrado.
 *
 * A marcação é composta pelo contorno do símbolo, marcas nos quatro cantos,
 * cruzes sobre os centros dos padrões de localização e uma etiqueta com a
 * numeração. O código selecionado recebe preenchimento translúcido.
 *
 * Todas as coordenadas chegam em pixels da imagem de entrada e são
 * convertidas para a janela por imagem_para_janela, a partir da área em que
 * a imagem foi efetivamente desenhada.
 */

/* Espessura do contorno: o raio 1 produz uma linha de 3 pixels. */
#define CONTORNO_RAIO_SELECIONADO 1
#define CONTORNO_RAIO_NORMAL      0

/* ------------------------------------------------------------------------- */
/* Primitivas de desenho                                                     */
/* ------------------------------------------------------------------------- */

static SDL_FColor cor_para_fcolor(SDL_Color cor, float alfa) {
    SDL_FColor resultado;

    resultado.r = (float)cor.r / 255.0f;
    resultado.g = (float)cor.g / 255.0f;
    resultado.b = (float)cor.b / 255.0f;
    resultado.a = alfa;

    return resultado;
}

/* Desenha um texto centralizado horizontalmente em `centro_x`. */
static void desenhar_texto_centralizado(
    SDL_Renderer *render,
    TTF_Font *fonte,
    const char *texto,
    float centro_x,
    float y,
    SDL_Color cor
) {
    int largura = 0;
    int altura = 0;

    TTF_GetStringSize(fonte, texto, 0, &largura, &altura);

    gui_desenhar_texto(
        render,
        fonte,
        texto,
        centro_x - (float)largura / 2.0f,
        y,
        cor
    );
}

/* Converte um ponto em coordenadas da imagem para coordenadas da janela. */
static SDL_FPoint imagem_para_janela(const Gui *gui, QrPonto ponto) {
    SDL_FPoint resultado = { 0.0f, 0.0f };

    if (gui->imagem_largura <= 0 || gui->imagem_altura <= 0) {
        return resultado;
    }

    const float escala_x = gui->area_imagem.w / (float)gui->imagem_largura;
    const float escala_y = gui->area_imagem.h / (float)gui->imagem_altura;

    resultado.x = gui->area_imagem.x + ponto.x * escala_x;
    resultado.y = gui->area_imagem.y + ponto.y * escala_y;

    return resultado;
}

/* Preenche um quadrilátero com uma cor translúcida (dois triângulos). */
static void preencher_quadrilatero(
    SDL_Renderer *render,
    const SDL_FPoint pontos[4],
    SDL_FColor cor
) {
    static const int indices[6] = { 0, 1, 2, 0, 2, 3 };

    SDL_Vertex vertices[6];

    for (int i = 0; i < 6; ++i) {
        vertices[i].position = pontos[indices[i]];
        vertices[i].color = cor;
        vertices[i].tex_coord.x = 0.0f;
        vertices[i].tex_coord.y = 0.0f;
    }

    SDL_RenderGeometry(render, NULL, vertices, 6, NULL, 0);
}

/*
 * Desenha o contorno fechado do quadrilátero. Como SDL_RenderLines traça
 * linhas de um pixel, o contorno é repetido com pequenos deslocamentos
 * para ganhar espessura.
 */
static void desenhar_contorno(
    SDL_Renderer *render,
    const SDL_FPoint pontos[4],
    int raio
) {
    SDL_FPoint caminho[5];

    for (int i = 0; i < 4; ++i) {
        caminho[i] = pontos[i];
    }

    caminho[4] = pontos[0];

    for (int dy = -raio; dy <= raio; ++dy) {
        for (int dx = -raio; dx <= raio; ++dx) {
            SDL_FPoint deslocado[5];

            for (int i = 0; i < 5; ++i) {
                deslocado[i].x = caminho[i].x + (float)dx;
                deslocado[i].y = caminho[i].y + (float)dy;
            }

            SDL_RenderLines(render, deslocado, 5);
        }
    }
}

/* Quadrado cheio centrado em um ponto, usado para marcar os cantos. */
static void marcar_canto(SDL_Renderer *render, SDL_FPoint ponto, float lado) {
    const SDL_FRect rect = {
        ponto.x - lado / 2.0f,
        ponto.y - lado / 2.0f,
        lado,
        lado
    };

    SDL_RenderFillRect(render, &rect);
}

/* Cruz com moldura, usada para marcar o centro de um padrão de localização. */
static void marcar_padrao(SDL_Renderer *render, SDL_FPoint ponto) {
    const float braco = 6.0f;

    SDL_RenderLine(
        render,
        ponto.x - braco,
        ponto.y,
        ponto.x + braco,
        ponto.y
    );

    SDL_RenderLine(
        render,
        ponto.x,
        ponto.y - braco,
        ponto.x,
        ponto.y + braco
    );

    const SDL_FRect moldura = {
        ponto.x - braco,
        ponto.y - braco,
        braco * 2.0f,
        braco * 2.0f
    };

    SDL_RenderRect(render, &moldura);
}

/* Etiqueta com fundo sólido, para numerar as regiões sobre a imagem. */
static void desenhar_etiqueta(
    Gui *gui,
    const char *texto,
    float x,
    float y,
    SDL_Color fundo
) {
    SDL_Renderer *render = gui->render_principal;

    int largura = 0;
    int altura = 0;

    TTF_GetStringSize(gui->fonte_pequena, texto, 0, &largura, &altura);

    const SDL_FRect caixa = {
        x,
        y,
        (float)largura + 10.0f,
        (float)altura + 6.0f
    };

    gui_definir_cor(render, fundo);
    SDL_RenderFillRect(render, &caixa);

    gui_desenhar_texto(
        render,
        gui->fonte_pequena,
        texto,
        caixa.x + 5.0f,
        caixa.y + 3.0f,
        COR_FUNDO_PRINCIPAL
    );
}

/* ------------------------------------------------------------------------- */
/* Marcação das regiões                                                      */
/* ------------------------------------------------------------------------- */

static void desenhar_marcacoes(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_principal;
    const QrResultado *resultado = dados->resultado;

    SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < resultado->quantidade; ++i) {
        const QrCodigo *codigo = &resultado->codigos[i];

        SDL_FPoint pontos[4];

        for (int c = 0; c < 4; ++c) {
            pontos[c] = imagem_para_janela(gui, codigo->cantos[c]);
        }

        const bool selecionado = (i == dados->selecionado);

        SDL_Color cor = COR_MARCA;

        if (!codigo->decodificado) {
            cor = COR_MARCA_FALHA;
        } else if (selecionado) {
            cor = COR_MARCA_SELECIONADA;
        }

        /* Preenchimento translúcido destaca o código selecionado. */
        if (selecionado) {
            preencher_quadrilatero(
                render,
                pontos,
                cor_para_fcolor(cor, 0.18f)
            );
        }

        gui_definir_cor(render, cor);

        desenhar_contorno(
            render,
            pontos,
            selecionado ? CONTORNO_RAIO_SELECIONADO : CONTORNO_RAIO_NORMAL
        );

        for (int c = 0; c < 4; ++c) {
            marcar_canto(render, pontos[c], 8.0f);
        }

        /* Padrões de localização encontrados pela detecção própria. */
        for (int p = 0; p < codigo->num_padroes; ++p) {
            marcar_padrao(render, imagem_para_janela(gui, codigo->padroes[p]));
        }

        char rotulo[64];

        if (codigo->decodificado) {
            SDL_snprintf(rotulo, sizeof(rotulo), "QR %d", i + 1);
        } else {
            SDL_snprintf(
                rotulo,
                sizeof(rotulo),
                "QR %d (não decodificado)",
                i + 1
            );
        }

        /* A etiqueta fica acima do contorno, ou abaixo se não houver espaço. */
        float etiqueta_y = pontos[0].y - 22.0f;

        if (etiqueta_y < gui->area_imagem.y) {
            etiqueta_y = pontos[0].y + 4.0f;
        }

        desenhar_etiqueta(gui, rotulo, pontos[0].x, etiqueta_y, cor);
    }

    SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_NONE);
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

void gui_desenhar_principal(Gui *gui, const GuiDados *dados) {
    SDL_Renderer *render = gui->render_principal;

    int janela_largura = 0;
    int janela_altura = 0;

    SDL_GetRenderOutputSize(render, &janela_largura, &janela_altura);

    gui_definir_cor(render, COR_FUNDO_PRINCIPAL);
    SDL_RenderClear(render);

    SDL_Texture *textura = gui->textura_original;

    if (dados != NULL &&
        dados->mostrar_binaria &&
        gui->textura_binaria != NULL) {

        textura = gui->textura_binaria;
    }

    if (textura == NULL || gui->imagem_largura <= 0 || gui->imagem_altura <= 0) {
        const float centro_x = (float)janela_largura / 2.0f;
        const float centro_y = (float)janela_altura / 2.0f;

        desenhar_texto_centralizado(
            render,
            gui->fonte_titulo,
            "Nenhuma imagem carregada",
            centro_x,
            centro_y - 40.0f,
            COR_TEXTO
        );

        desenhar_texto_centralizado(
            render,
            gui->fonte,
            "Use o botão \"Abrir imagem\" na janela de leitura (tecla O)",
            centro_x,
            centro_y,
            COR_TEXTO_SUAVE
        );

        desenhar_texto_centralizado(
            render,
            gui->fonte,
            "ou arraste um arquivo de imagem para esta janela.",
            centro_x,
            centro_y + 24.0f,
            COR_TEXTO_SUAVE
        );

        SDL_RenderPresent(render);

        return;
    }

    /* Ajusta a imagem à janela mantendo a proporção e centraliza. */
    const float escala_x = (float)janela_largura / (float)gui->imagem_largura;
    const float escala_y = (float)janela_altura / (float)gui->imagem_altura;
    const float escala = (escala_x < escala_y) ? escala_x : escala_y;

    gui->area_imagem.w = (float)gui->imagem_largura * escala;
    gui->area_imagem.h = (float)gui->imagem_altura * escala;
    gui->area_imagem.x = ((float)janela_largura - gui->area_imagem.w) / 2.0f;
    gui->area_imagem.y = ((float)janela_altura - gui->area_imagem.h) / 2.0f;

    SDL_RenderTexture(render, textura, NULL, &gui->area_imagem);

    if (dados != NULL &&
        dados->mostrar_marcacao &&
        dados->resultado != NULL &&
        dados->resultado->quantidade > 0) {

        desenhar_marcacoes(gui, dados);
    }

    SDL_RenderPresent(render);
}

SDL_Surface *gui_capturar_imagem_marcada(Gui *gui, const GuiDados *dados) {
    if (gui->textura_original == NULL ||
        gui->imagem_largura <= 0 ||
        gui->imagem_altura <= 0) {

        fprintf(stderr, "Erro: nenhuma imagem carregada para salvar.\n");

        return NULL;
    }

    SDL_Texture *textura = gui->textura_original;

    if (dados != NULL &&
        dados->mostrar_binaria &&
        gui->textura_binaria != NULL) {

        textura = gui->textura_binaria;
    }

    /*
     * Desenhar em uma textura do tamanho original evita depender do
     * conteúdo da janela, que já foi apresentado, e grava o resultado na
     * resolução do arquivo de entrada em vez da resolução da janela.
     */
    SDL_Texture *alvo = SDL_CreateTexture(
        gui->render_principal,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        gui->imagem_largura,
        gui->imagem_altura
    );

    if (alvo == NULL) {
        fprintf(
            stderr,
            "Erro ao criar a textura de saida: %s\n",
            SDL_GetError()
        );

        return NULL;
    }

    if (!SDL_SetRenderTarget(gui->render_principal, alvo)) {
        fprintf(
            stderr,
            "Erro ao redirecionar o desenho: %s\n",
            SDL_GetError()
        );

        SDL_DestroyTexture(alvo);

        return NULL;
    }

    const SDL_FRect area_anterior = gui->area_imagem;

    gui->area_imagem.x = 0.0f;
    gui->area_imagem.y = 0.0f;
    gui->area_imagem.w = (float)gui->imagem_largura;
    gui->area_imagem.h = (float)gui->imagem_altura;

    gui_definir_cor(gui->render_principal, COR_FUNDO_PRINCIPAL);
    SDL_RenderClear(gui->render_principal);

    SDL_RenderTexture(gui->render_principal, textura, NULL, &gui->area_imagem);

    if (dados != NULL &&
        dados->mostrar_marcacao &&
        dados->resultado != NULL &&
        dados->resultado->quantidade > 0) {

        desenhar_marcacoes(gui, dados);
    }

    SDL_Surface *captura = SDL_RenderReadPixels(gui->render_principal, NULL);

    SDL_SetRenderTarget(gui->render_principal, NULL);
    SDL_DestroyTexture(alvo);

    gui->area_imagem = area_anterior;

    if (captura == NULL) {
        fprintf(
            stderr,
            "Erro ao capturar a imagem marcada: %s\n",
            SDL_GetError()
        );
    }

    return captura;
}
