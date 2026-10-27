# Proj2 — Detecção e leitura de QR Codes (Computação Visual)

Aplicação em C que recebe uma imagem, localiza os QR Codes presentes nela,
destaca visualmente a região de cada código encontrado e exibe o conteúdo
decodificado.

A localização do código na imagem é feita pelo próprio projeto, a partir do
processamento da imagem. A biblioteca externa (ZBar) é usada apenas na etapa
de decodificação do conteúdo, conforme previsto no escopo do trabalho.

## Integrantes

| Integrante | RA | Parte |
|---|---|---|
| André Cizotti | 10409439 | Processamento de imagem: detecção e conversão para escala de cinza, histograma e estatísticas, limiar de Otsu, binarização adaptativa e detecção dos padrões de localização (`image_loader`, `image_processing`, `qr_detector`) |
| Pedro de Souza Zequi | 10419805 | Interatividade: alternância entre a imagem binarizada e a original, controle de resolução, seleção e cópia do conteúdo, salvamento da imagem marcada e integração da decodificação (`qr_decoder`, `image_saver`, `analise`, `main`) |
| Luis Fernando de Mesquita Pereira | 10410686 | Interface gráfica: janela principal, janela secundária, desenho da marcação dos códigos e textos com SDL_ttf (`gui`, `gui_imagem`, `gui_painel`) |

## Executável pronto (Windows x64)

Não é necessário compilar para usar a aplicação. O executável para Windows,
já acompanhado de todas as DLLs, está publicado como
[release V1.0.0 do repositório](https://github.com/luisfernandomp/proj-comp-visual-2/releases/tag/V1.0.0).

Baixe o arquivo `Projeto-QR-Code-Windows.zip`, descompacte e abra `proj2.exe` — por
duplo clique ou pelo terminal, opcionalmente passando o caminho de uma
imagem:

```
proj2.exe exemplos\qr_simples.png
```

Nenhuma instalação é necessária. A pasta `assets/` precisa continuar ao lado
do executável, porque a fonte da interface é carregada dela.

O arquivo [INTEGRANTES.txt](INTEGRANTES.txt) traz a identificação do grupo, as
referências consultadas e o processo de compilação em forma de texto, como
pede o enunciado.

## Como compilar e executar

Requisitos: GCC (C99+), SDL3, SDL3_image, SDL3_ttf e ZBar instaladas e
visíveis pelo `pkg-config`.

No Windows, com MSYS2 (ambiente UCRT64):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-pkgconf \
          mingw-w64-ucrt-x86_64-make \
          mingw-w64-ucrt-x86_64-SDL3 \
          mingw-w64-ucrt-x86_64-SDL3_image \
          mingw-w64-ucrt-x86_64-SDL3_ttf \
          mingw-w64-ucrt-x86_64-zbar
```

No Linux (Debian/Ubuntu), o ZBar vem como `libzbar-dev`; as bibliotecas SDL3
ainda não estão empacotadas em todas as distribuições e podem precisar ser
compiladas a partir do código-fonte.

Compilação e execução:

```bash
make
./proj2 exemplos/qr_simples.png
```

No Windows/MSYS2 o executável gerado é `proj2.exe` e o comando de compilação
é `mingw32-make`.

O caminho da imagem é opcional: sem argumento, a aplicação abre com as duas
janelas vazias e a imagem pode ser escolhida pelo botão **Abrir imagem** ou
arrastada para a janela.

Se a compilação falhar com `Package 'sdl3' not found`, o `pkg-config` não está
encontrando as bibliotecas. Os quatro comandos abaixo precisam responder com
um número de versão; no Windows, uma falha aqui costuma indicar que o shell
aberto não foi o UCRT64.

```bash
pkg-config --modversion sdl3 sdl3-image sdl3-ttf zbar
```

### Gerando o pacote distribuível

O executável compilado no MSYS2 depende de cerca de vinte DLLs do ambiente e
não roda, sozinho, em uma máquina sem o MSYS2 instalado. O script
`scripts/empacotar-windows.sh` monta o pacote completo:

```bash
mingw32-make dist
```

Ele resolve as dependências do executável com o `objdump`, recursivamente,
copia do MSYS2 apenas as DLLs realmente usadas, junta a fonte, os exemplos e
a documentação, e compacta o resultado em `dist/Projeto-QR-Code-Windows.zip` — que é
o arquivo anexado à release. Se o MSYS2 estiver em outro caminho, informe o
diretório das DLLs:

```bash
MSYS_BIN=/d/msys64/ucrt64/bin bash scripts/empacotar-windows.sh
```

## Como usar

A aplicação abre duas janelas:

- **Janela principal** — mostra a imagem com a região de cada QR Code
  destacada: o contorno do símbolo, marcas nos quatro cantos, cruzes sobre os
  três padrões de localização e a numeração de cada código. O código
  selecionado aparece em verde, com preenchimento translúcido; os demais, em
  azul. Um código localizado mas não decodificado aparece em laranja.
- **Janela de leitura** — mostra as informações da análise, o conteúdo
  decodificado do código selecionado e os botões de interação.

| Ação | Botão | Tecla |
|---|---|---|
| Escolher uma imagem | Abrir imagem | `O` |
| Alternar entre a imagem original e a binarizada | Ver imagem binarizada | `B` |
| Mostrar ou ocultar a marcação | Mostrar/Ocultar marcação | `M` |
| Selecionar o próximo QR Code da imagem | Próximo QR Code | `Tab` |
| Copiar o conteúdo para a área de transferência | Copiar conteúdo | `C` |
| Salvar a imagem com a marcação em `qr_resultado.png` | Salvar imagem marcada | `S` |
| Alternar entre 1024x768 e a resolução da imagem | — | `R` |
| Sair | — | `Esc` |

Também é possível **arrastar um arquivo de imagem** para qualquer uma das
janelas, **clicar sobre um código destacado** para selecioná-lo e usar a
**roda do mouse** sobre o painel para rolar conteúdos longos.

A imagem salva com `S` é gerada na resolução original do arquivo de entrada,
e não na resolução da janela.

## Estrutura do projeto

```
proj2/
├── src/
│   ├── main.c                 # estado da aplicação e ligação entre as partes
│   ├── analise.h/.c           # encadeia as etapas de processamento da imagem
│   ├── image_loader.h/.c      # carregamento da imagem
│   ├── image_processing.h/.c  # escala de cinza, histograma e binarização
│   ├── qr_tipos.h/.c          # tipos e geometria compartilhados
│   ├── qr_detector.h/.c       # localização dos QR Codes na imagem
│   ├── qr_decoder.h/.c        # decodificação do conteúdo (ZBar)
│   ├── image_saver.h/.c       # gravação da imagem marcada
│   ├── gui.h                  # interface pública da interface gráfica
│   ├── gui_interno.h          # paleta e utilitários internos da interface
│   ├── gui.c                  # janelas, fontes, texturas e eventos
│   ├── gui_imagem.c           # janela principal: imagem e marcação
│   └── gui_painel.c           # janela de leitura: informações e botões
├── exemplos/                  # imagens de teste
├── assets/
│   └── fonts/                 # DejaVu Sans, usada pelo SDL_ttf
├── scripts/
│   └── empacotar-windows.sh   # monta o pacote executável + DLLs
├── Makefile                   # compila os arquivos em src/*.c
├── INTEGRANTES.txt            # grupo, referências e processo de compilação
└── README.md
```

Cada arquivo tem uma responsabilidade única e as dependências vão sempre na
mesma direção: `main.c` conhece `analise` e `gui`, `analise` conhece os
módulos de processamento, e nenhum módulo de processamento conhece a
interface. Os três arquivos da interface compartilham apenas o que está em
`gui_interno.h`; o restante do programa enxerga somente `gui.h`.

## Como funciona

Para cada imagem selecionada, a aplicação executa cinco etapas.

### 1. Carregamento (`image_loader`)

A imagem é lida com `IMG_Load` e convertida para RGBA32, formato usado por
todas as etapas seguintes. Erros de arquivo inexistente ou formato inválido
são tratados e informados.

### 2. Pré-processamento (`image_processing`)

A imagem é convertida para escala de cinza pela luminância perceptual:

```
Y = 0.2125 * R + 0.7154 * G + 0.0721 * B
```

Em seguida é binarizada. São implementados dois métodos:

- **Binarização adaptativa pela média local**: cada pixel é comparado com a
  média de uma janela quadrada centrada nele, descontada de um valor fixo.
  A média é calculada em tempo constante por pixel com uma **imagem
  integral**, o que mantém o custo linear no total de pixels,
  independentemente do tamanho da janela. É mais robusta em fotografias com
  iluminação irregular ou sombra sobre o código.
- **Binarização global pelo método de Otsu**: o limiar é escolhido a partir
  do histograma de 256 níveis, maximizando a variância entre as duas classes
  de intensidade. Funciona bem em imagens sintéticas e capturas de tela.

A detecção é tentada primeiro sobre a binarização adaptativa. Se nenhuma
região for encontrada, a imagem é binarizada de novo com o limiar de Otsu e a
detecção é repetida. A janela de leitura informa qual dos dois métodos
produziu o resultado exibido.

### 3. Detecção (`qr_detector`)

Esta é a etapa de Computação Visual propriamente dita, implementada pelo
projeto. Ela se baseia nos **padrões de localização** do QR Code: os três
quadrados concêntricos nos cantos do símbolo. Qualquer reta que passe pelo
centro de um desses padrões o corta na sequência
escuro-claro-escuro-claro-escuro, na proporção **1:1:3:1:1**.

1. **Varredura das linhas.** Cada linha da imagem binarizada é percorrida em
   sequências de pixels de mesma cor. Janelas de cinco sequências
   consecutivas que comecem em uma faixa escura são testadas contra a
   proporção 1:1:3:1:1. Como o padrão tem 7 módulos de largura, a soma das
   cinco faixas dividida por 7 dá o tamanho estimado do módulo.
2. **Confirmação cruzada.** Cada candidato é reconferido na vertical, na
   coluna do centro encontrado, e novamente na horizontal, na linha
   refinada. Isso descarta faixas de texto e molduras, que satisfazem a
   proporção em um eixo mas não no outro.
3. **Agrupamento.** O mesmo padrão aparece em várias linhas vizinhas:
   candidatos próximos e de módulo semelhante são fundidos em um único
   registro, cujo centro é a média dos centros acumulados. Padrões
   confirmados em pelo menos duas linhas são mantidos.
4. **Combinação em trios.** Os três padrões de um QR Code formam um
   triângulo retângulo isósceles, cujo vértice do ângulo reto é o canto
   superior esquerdo do símbolo. Cada trio é testado quanto à semelhança dos
   módulos, à igualdade dos catetos, ao teorema de Pitágoras e à distância
   mínima e máxima entre centros. O produto vetorial entre os dois catetos
   indica qual padrão está à direita e qual está abaixo.
5. **Reconstrução da região.** A distância entre os centros equivale a
   (`módulos` - 7) módulos, o que dá a versão do símbolo — todo QR Code tem
   `4 * versão + 17` módulos de lado. Com a versão fixada, o tamanho do
   módulo é recalculado a partir da distância entre os centros, o que evita
   o erro que se acumularia ao multiplicar o módulo medido pelo índice de
   cada módulo. O quarto canto, que não tem padrão de localização, é obtido
   completando o paralelogramo, e os quatro cantos do símbolo saem do
   deslocamento de 3,5 módulos para fora de cada centro.
6. **Verificação estrutural.** O trio só é aceito se a região contiver o
   **padrão de temporização** do QR Code: a linha 6 e a coluna 6 do símbolo
   alternam módulos escuros e claros, sendo escuro todo módulo de índice par.
   Essa verificação é o que distingue um QR Code real de três padrões que,
   por acaso, formaram a geometria esperada — situação que ocorre, por
   exemplo, com dois QR Codes lado a lado na mesma imagem.

### 4. Decodificação (`qr_decoder`)

Única etapa delegada a uma biblioteca externa. A imagem em escala de cinza é
entregue à **ZBar** no formato de um canal (Y800), que faz a sua própria
binarização interna, a correção de erros Reed-Solomon, a remoção da máscara e
a decodificação dos modos de codificação.

Cada símbolo lido é associado, pela posição do seu centro, à região já
localizada na etapa anterior. Um símbolo que a biblioteca leia fora de
qualquer região detectada é acrescentado ao resultado e identificado na
interface como vindo da biblioteca, e não da detecção própria.

### 5. Apresentação (`gui`, `gui_imagem`, `gui_painel`)

A janela principal ajusta a imagem à área disponível mantendo a proporção e
desenha a marcação convertendo as coordenadas da imagem para as da janela. A
janela de leitura mostra as informações da análise e o conteúdo decodificado,
quebrado em linhas pelo SDL_ttf.

O conteúdo de um QR Code é uma sequência arbitrária de bytes: antes de ser
exibido, ele é filtrado para trocar caracteres de controle por espaço e bytes
que não formam UTF-8 válido por `?`, já que o SDL_ttf não renderiza texto mal
formado. O que é copiado para a área de transferência são os bytes originais.

## Parâmetros adotados

O enunciado não fixa valores, então foram adotados os seguintes limiares de
implementação, todos reunidos no topo dos respectivos arquivos:

| Parâmetro | Valor | Onde |
|---|---|---|
| Tolerância da proporção 1:1:3:1:1 | 0,5 módulo por faixa | `qr_detector.c` |
| Confirmações mínimas de um padrão | 2 linhas (1 se não houver trio) | `qr_detector.c` |
| Diferença máxima entre os catetos | 30% do maior | `qr_detector.c` |
| Erro máximo no teorema de Pitágoras | 20% da hipotenusa | `qr_detector.c` |
| Distância entre centros | de 10 a 180 módulos | `qr_detector.c` |
| Acerto mínimo no padrão de temporização | 75% dos módulos | `qr_detector.c` |
| Janela da binarização adaptativa | menor lado / 16, entre 15 e 99 px | `image_processing.c` |
| Desvio da binarização adaptativa | 7 níveis abaixo da média local | `analise.c` |
| Máximo de QR Codes por imagem | 8 | `qr_tipos.h` |
| Máximo de bytes por conteúdo | 2048 | `qr_tipos.h` |

## Imagens de exemplo

A pasta `exemplos/` traz casos de teste gerados com `qrencode`:

| Arquivo | O que demonstra |
|---|---|
| `qr_simples.png` | código isolado, com margem branca |
| `qr_em_cena.png` | código dentro de uma imagem maior, com texto ao redor |
| `qr_rotacionado.png` | código girado 18°: o contorno acompanha a rotação |
| `dois_qr_codes.png` | dois códigos na mesma imagem, selecionáveis com `Tab` |
| `qr_texto_longo.png` | conteúdo longo, para a rolagem do painel |
| `sem_qr_code.png` | caso negativo: nenhuma região deve ser marcada |

## Limitações conhecidas

- A detecção assume que o símbolo é aproximadamente plano. Rotação e
  perspectiva leve são tratadas; fotografias com perspectiva acentuada podem
  ter a região reconstruída com imprecisão nas extremidades, ainda que o
  conteúdo seja decodificado.
- São exibidos no máximo 8 QR Codes por imagem.
- A versão do SDL3_image usada (3.4.x) tem duas limitações que o projeto
  contorna em `image_loader.c`:
  - um PNG em tons de cinza com 16 bits por canal encerra o processo dentro
    do `IMG_Load`, sem retornar erro. O cabeçalho do arquivo é conferido
    antes da leitura e esse caso é recusado com uma mensagem explicando que a
    imagem precisa ser convertida para 8 bits por canal;
  - um PNG em tons de cinza com menos de 8 bits por pixel é carregado com uma
    paleta que não chega ao branco, deixando a imagem acinzentada. A paleta é
    reescalada após o carregamento. A detecção não é afetada, porque toda a
    análise é feita por limiar.

## Créditos

A fonte DejaVu Sans, distribuída em `assets/fonts/`, segue a licença incluída
no mesmo diretório. A decodificação do conteúdo usa a biblioteca
[ZBar](https://github.com/mchehab/zbar) (LGPL-2.1).
