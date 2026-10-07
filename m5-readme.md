# Exercício do Módulo 5 - Câmera em primeira pessoa

Trabalho da disciplina de Computação Gráfica (Unisinos).

Dando continuidade ao visualizador, a cena agora é explorada por uma **câmera
em primeira pessoa**. A câmera é um objeto de uma classe `Camera`, que agrupa
todos os seus atributos e encapsula as ações de **Mover** e **Rotacionar**,
como pede o material de aprofundamento do módulo.

## O que foi implementado nesta atividade

- Classe `Camera`, num arquivo próprio (`Camera.h`), com a posição, a
  orientação (`front`, `up`, `right`), os ângulos de Euler (`yaw`, `pitch`) e
  os parâmetros do frustum (FOV, near, far)
- Movimentação pelo teclado (W, A, S, D), com a velocidade moderada pelo
  `deltaTime`, para ficar igual em qualquer taxa de quadros
- Rotação da câmera pelo mouse, olhando ao redor pelos ângulos `yaw` e `pitch`
- Zoom pelo scroll do mouse, alterando o FOV
- A matriz de view e a de projeção passam a ser geradas pela câmera a cada
  quadro, no lugar do `lookAt` e do `perspective` fixos que a cena usava antes

## A classe Camera

A câmera guarda tudo o que precisa e expõe as ações por métodos, de modo que o
programa principal nunca mexe direto nos vetores:

| Atributo               | Para que serve                           |
| ---------------------- | ---------------------------------------- |
| `position`             | onde a câmera está no mundo              |
| `front`, `up`, `right` | para onde ela olha e como está orientada |
| `yaw`, `pitch`         | ângulos de Euler que geram o `front`     |
| `fov`, `zNear`, `zFar` | definem o frustum (o volume visível)     |
| `speed`, `sensitivity` | velocidade de deslocamento e do mouse    |

| Método                         | Ação                                         |
| ------------------------------ | -------------------------------------------- |
| `mover(direcao, deltaTime)`    | anda para frente, trás, esquerda ou direita  |
| `rotacionar(xoffset, yoffset)` | muda `yaw`/`pitch` e recalcula os vetores    |
| `processarMouse(x, y)`         | converte a posição do cursor em rotação      |
| `zoom(yoffset)`                | aproxima ou afasta alterando o FOV           |
| `getViewMatrix()`              | devolve a matriz de view (`lookAt`)          |
| `getProjectionMatrix(aspect)`  | devolve a matriz de projeção (`perspective`) |

## Como a câmera se movimenta e gira

A **view matrix** é montada a cada quadro com
`lookAt(position, position + front, up)`: a câmera olha sempre para um ponto à
sua frente, e não para um alvo fixo. Mover é somar à `position` um dos vetores
de orientação; girar é mudar os ângulos e recalcular o `front`:

```cpp
front.x = cos(radians(yaw)) * cos(radians(pitch));
front.y = sin(radians(pitch));
front.z = sin(radians(yaw)) * cos(radians(pitch));
```

O `pitch` é travado em ±89° para a câmera não virar de cabeça para baixo. O
`deltaTime` (tempo entre um quadro e o outro) multiplica a velocidade, para o
movimento não depender do computador ser mais rápido ou mais lento.

## Mantido dos módulos anteriores

Leitura da geometria, das coordenadas de textura e das normais do `.OBJ`;
leitura dos coeficientes e da textura do `.MTL`; modelo de Phong com as
parcelas ambiente, difusa e especular; iluminação de três pontos com
atenuação; dois objetos na cena com seleção e transformações. Como agora a
`cameraPos` é enviada ao shader a cada quadro, o brilho especular acompanha a
câmera enquanto você anda pela cena.

## Como compilar e rodar

Precisa de CMake e um compilador C++. Usamos o MSYS2 com o VS Code no Windows.
O CMake baixa a GLFW, a GLM e a stb_image sozinho.

```bash
git clone <link-do-repositorio>
cd <pasta-do-projeto>
cmake -S . -B build
cmake --build build
```

Para rodar, entre na pasta `build` (senão o programa não acha os modelos):

```bash
cd build
./Exercicio_M5        # no Windows: .\Exercicio_M5.exe
```

O `Camera.h` fica na mesma pasta do `.cpp`. Os arquivos do modelo ficam em
`assets/Modelos3D/` e precisam estar juntos: `Suzanne.obj`, `Suzanne.mtl` e
`Suzanne.png`.

## Controles

| Tecla         | O que faz                                          |
| ------------- | -------------------------------------------------- |
| W / A / S / D | move a câmera (frente / esquerda / trás / direita) |
| Mouse         | olha ao redor                                      |
| Scroll        | zoom (aproxima e afasta)                           |
| TAB           | alterna o objeto selecionado                       |
| R / T / E     | modo rotação / translação / escala                 |
| X / Y / Z     | aplica a transformação no eixo escolhido           |
| Shift + eixo  | inverte o sentido                                  |
| U             | liga/desliga a escala uniforme                     |
| Setas         | translada o objeto nos eixos X e Y                 |
| 1 / 2 / 3     | liga e desliga a luz key / fill / back             |
| Espaço        | volta a cena para o estado inicial                 |
| P             | alterna entre malha preenchida e wireframe         |
| ESC           | fecha o programa                                   |

Com o cursor escondido, o mouse não fecha a janela; use o ESC para sair.

## Referências

- Material de apoio do Módulo 5 (Câmera Sintética)
- [LearnOpenGL - Camera](https://learnopengl.com/Getting-started/Camera)
- [stb_image](https://github.com/nothings/stb)

---

Alunas: Eduarda Fernandes e Maria Eduarda Dias
