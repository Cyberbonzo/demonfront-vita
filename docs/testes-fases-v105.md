# Demon Front V105 â€” teste dos seis capÃ­tulos no ROG Ally

## Revisão instalada em 23 de novembro de 2026

Repetimos os seis capítulos no ROG Ally com a revisão que corrige as laterais. Os 26 mapas de campanha e os dois auxiliares produziram imagem. Os nove combates terminaram com o indicador de vitória e a transição para o mapa esperado. A bateria gerou 115 capturas nas entradas de mapa e 81 nos combates. O executável nativo testado tem SHA-256 `72212b9fbda31fdec62e04e4e18cf124eafd780d222a014215409a3b1a70e21c`.

Nos mesmos quadros dos dez mapas com defeito, comparamos a faixa direita antes e depois. O cenário passou a ocupar os espaços vazios do piso, do céu e das arenas. A composição usa os blocos e panoramas da própria fase. Onde a arte termina, a extensão repete ou reflete somente o cenário. A saída da floresta usa a mesma altura de terreno dos dois lados da emenda. O clarão de ataque do mapa 50 também acompanha a largura inteira, inclusive com o deslocamento da câmera. Conferimos esse efeito no quadro 720 do combate.

O centro do personagem atingiu X=20 e X=578 em áreas abertas de um campo de 598 colunas. Essa margem mantém o corpo na imagem. As passagens que dependem de plataformas ou de um encontro continuam sujeitas às regras da fase. As capturas vieram de um framebuffer de 1920 × 1080 na GPU AMD do Ally. A taxa de apresentação foi amostrada separadamente na abertura do executável em uma sessão isolada do Gamescope.

Os testes usaram entradas programadas e entrada direta em cada mapa. As medições de pixels no CSV cobrem o quarto direito das imagens de 480 × 270 usadas para comparação. O preto também faz parte de sombras, efeitos e transições da arte original. A inspeção visual acompanhou a medição. A bateria não foi uma partida contínua do início aos créditos.

[Resultados da revisão por mapa](v105-wide-retest.csv)

**Versão anterior.** os 26 mapas dos capÃ­tulos e dois mapas auxiliares carregaram e produziram quadros de gameplay na GPU do Ally. O jogo sinalizou vitÃ³ria nos cinco chefes de capÃ­tulo e nas quatro variantes do confronto final. A composiÃ§Ã£o 16:9 apresentou falhas visuais reproduzÃ­veis em pelo menos oito mapas dos capÃ­tulos e nos dois auxiliares.

## Primeira bateria e quadros de reprodução

| CapÃ­tulo | Mapas exercitados (IDs internos) | VitÃ³ria e transiÃ§Ã£o observadas | Falhas visuais confirmadas na amostra |
|---|---|---|---|
| 1 | 0â€“2 | 1 â†’ 10 | O fundo do mapa 2 termina antes da borda direita. |
| 2 | 10â€“14 | 13 â†’ 20 | Fundo incompleto Ã  direita nos mapas 11, 12 e 14 e na arena 13 durante o combate. |
| 3 | 20â€“23 | 22 â†’ 30 | As capturas amostradas nÃ£o mostraram a mesma falha de borda. |
| 4 | 30â€“34 | 33 â†’ 40 | A lateral do mapa 34 mostra um bloco plano acima de uma faixa escura. |
| 5 | 40â€“44 | 43 â†’ 50 | No mapa 44, os objetos aparecem sobre a faixa preta Ã  direita. |
| 6 | 50â€“53 | 50 â†’ 51, 51 â†’ 52, 52 â†’ 53, 53 â†’ 0 | No mapa 50, parte do cÃ©u acaba num retÃ¢ngulo preto no canto superior direito. |
| Auxiliares | 62 e 63 | Entrada direta e imagem conferidas | No 62 hÃ¡ descontinuidade no piso e no cÃ©u. No 63 a faixa Ã  direita fica preta. |

A seta indica a mudanÃ§a do ID de mapa apÃ³s o jogo registrar a vitÃ³ria. As sessÃµes entraram diretamente nos mapas, por isso o retorno ao ID 0 no Ãºltimo teste nÃ£o certifica uma sequÃªncia de crÃ©ditos de campanha.

## MÃ©todo

- A bateria rodou em processos separados, com as bibliotecas recompiladas e o compositor OpenGL da versÃ£o instalada no Ally. O executÃ¡vel de referÃªncia tem SHA-256 `1eb1fee5de499a4ace0d948ca5709f0f35fdb31d2b1b022e662270bf6de3f2f8`.
- O seletor de teste alterou o ID antes da inicializaÃ§Ã£o da sala. Em cada caso, verificamos o descritor de mapa carregado, o estado de jogo, as coordenadas do jogador e a saÃ­da da GPU. A primeira bateria gerou 115 amostras do framebuffer de 1920 Ã— 1080, reduzidas para inspeÃ§Ã£o. O mapa 60 Ã© o seletor de fase.
- Os primeiros testes usaram movimentaÃ§Ã£o, tiros e saltos programados. Nas lutas de chefe, o processo de teste aplicou proteÃ§Ã£o e muniÃ§Ã£o para exercitar as transiÃ§Ãµes. Cada uma das nove vitÃ³rias foi confirmada pelo indicador de conclusÃ£o do jogo seguido da mudanÃ§a de mapa. O combate permaneceu a cargo do jogo.
- O relÃ³gio da simulaÃ§Ã£o foi acelerado no processo de teste. Os resultados medem cobertura funcional e visual. A taxa de apresentaÃ§Ã£o por capÃ­tulo nÃ£o foi medida nesta bateria. O executÃ¡vel instalado e o save do usuÃ¡rio permaneceram intactos.
- O CSV identifica os quadros de reproduÃ§Ã£o. Um campo vazio de defeito significa que nÃ£o registramos um exemplo confirmado nesse mapa. Os valores de X sÃ£o os extremos observados durante as entradas de teste. ObstÃ¡culos, cÃ¢mera e trajetÃ³rias variam por sala.

[Resultados por mapa](v105-stage-coverage.csv)
