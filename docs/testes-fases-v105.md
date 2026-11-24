# Demon Front V105 — teste dos seis capítulos no ROG Ally

**Resultado funcional:** os 26 mapas dos capítulos e dois mapas auxiliares carregaram e produziram quadros de gameplay na GPU do Ally. O jogo sinalizou vitória nos cinco chefes de capítulo e nas quatro variantes do confronto final. A composição 16:9 apresentou falhas visuais reproduzíveis em pelo menos oito mapas dos capítulos e nos dois auxiliares.

## Cobertura

| Capítulo | Mapas exercitados (IDs internos) | Vitória e transição observadas | Falhas visuais confirmadas na amostra |
|---|---|---|---|
| 1 | 0–2 | 1 → 10 | O fundo do mapa 2 termina antes da borda direita. |
| 2 | 10–14 | 13 → 20 | Fundo incompleto à direita nos mapas 11, 12 e 14 e na arena 13 durante o combate. |
| 3 | 20–23 | 22 → 30 | As capturas amostradas não mostraram a mesma falha de borda. |
| 4 | 30–34 | 33 → 40 | A lateral do mapa 34 mostra um bloco plano acima de uma faixa escura. |
| 5 | 40–44 | 43 → 50 | No mapa 44, os objetos aparecem sobre a faixa preta à direita. |
| 6 | 50–53 | 50 → 51, 51 → 52, 52 → 53, 53 → 0 | No mapa 50, parte do céu acaba num retângulo preto no canto superior direito. |
| Auxiliares | 62 e 63 | Entrada direta e imagem conferidas | No 62 há descontinuidade no piso e no céu. No 63 a faixa à direita fica preta. |

A seta indica a mudança do ID de mapa após o jogo registrar a vitória. As sessões entraram diretamente nos mapas, por isso o retorno ao ID 0 no último teste não certifica uma sequência de créditos de campanha.

## Método

- A bateria rodou em processos separados, com as bibliotecas recompiladas e o compositor OpenGL da versão instalada no Ally. O executável de referência tem SHA-256 `1eb1fee5de499a4ace0d948ca5709f0f35fdb31d2b1b022e662270bf6de3f2f8`.
- O seletor de teste alterou o ID antes da inicialização da sala. Em cada caso, verificamos o descritor de mapa carregado, o estado de jogo, as coordenadas do jogador e a saída da GPU. A primeira bateria gerou 115 amostras do framebuffer de 1920 × 1080, reduzidas para inspeção. O mapa 60 é o seletor de fase.
- Os primeiros testes usaram movimentação, tiros e saltos programados. Nas lutas de chefe, o processo de teste aplicou proteção e munição para exercitar as transições. Cada uma das nove vitórias foi confirmada pelo indicador de conclusão do jogo seguido da mudança de mapa. O combate permaneceu a cargo do jogo.
- O relógio da simulação foi acelerado no processo de teste. Os resultados medem cobertura funcional e visual. A taxa de apresentação por capítulo não foi medida nesta bateria. O executável instalado e o save do usuário permaneceram intactos.
- O CSV identifica os quadros de reprodução. Um campo vazio de defeito significa que não registramos um exemplo confirmado nesse mapa. Os valores de X são os extremos observados durante as entradas de teste. Obstáculos, câmera e trajetórias variam por sala.

[Resultados por mapa](v105-stage-coverage.csv)
