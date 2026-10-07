/*
 * Exercício M5 - Câmera em primeira pessoa
 *
 * Objetivos:
 * - Câmera implementada como objeto da classe Camera (Camera.h), que agrupa
 *   seus atributos (posição, orientação, frustum) e encapsula as ações de
 *   mover e rotacionar;
 * - Movimento pelo teclado com velocidade controlada pelo deltaTime;
 * - Rotação pelo mouse (ângulos de Euler yaw e pitch) e zoom pelo scroll (FOV);
 * - Mantidos da atividade anterior: modelo de Phong, iluminação de três pontos
 *   com atenuação, seleção e transformação dos objetos.
 *
 * Teclas utilizadas:
 * - Seleção do objeto ==> TAB (o não selecionado fica escurecido)
 * - Modo rotação ==> 'R'
 * - Modo translação ==> 'T'
 * - Modo escala ==> 'E'
 * - Eixo da transformação ==> 'X', 'Y' e 'Z'
 * - Sentido inverso ==> SHIFT + eixo
 * - Escala uniforme (liga/desliga) ==> 'U'
 * - Translação em x e y ==> setas
 * - Reset da cena ==> ESPAÇO
 * - Malha preenchida / wireframe ==> 'P'
 * - Sair ==> ESC
 * - Liga/desliga luz key, fill e back ==> '1', '2' e '3'
 * - Mover a câmera ==> W, A, S, D
 * - Olhar ao redor ==> mouse
 * - Zoom ==> scroll do mouse
 *
 * Alunas: Eduarda Fernandes e Maria Eduarda Dias
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Camera.h"

using namespace std;

const GLuint WIDTH = 1000, HEIGHT = 800;

Camera camera(glm::vec3(0.0f, 0.0f, 7.0f));   // câmera começa em z = 7, olhando para a cena
float deltaTime = 0.0f, lastFrame = 0.0f;

struct Object3D
{
    GLuint VAO;
    int nVertices;
    glm::vec3 position;   // translacao
    glm::vec3 scale;      // escala
    glm::vec3 rotation;   // angulos em graus, um por eixo
    glm::vec3 posicaoInicial;
};

struct Material
{
    glm::vec3 ka = glm::vec3(0.2f);   // ambiente
    glm::vec3 kd = glm::vec3(1.0f);   // difusa
    glm::vec3 ks = glm::vec3(0.5f);   // especular
    float ns = 32.0f;                 // expoente especular (q)
    string textura = "";
};

struct Luz
{
    glm::vec3 direcao;     // para que lado do objeto ela fica (normalizada)
    float distancia;       // múltiplo do tamanho do objeto
    glm::vec3 cor;
    float intensidade;
    bool ligada = true;
};

// esquema clássico de 3 pontos: câmera em +z, então "frente" é +z, o lado onde a câmera começa
Luz luzes[3] = {
    // 0 - KEY: frente, à direita e acima. A mais forte, define a cena
    { glm::normalize(glm::vec3( 1.0f,  0.8f,  1.0f)), 3.0f, glm::vec3(1.0f, 0.97f, 0.9f),  1.6f, true },
    // 1 - FILL: frente, à esquerda, quase na altura dos olhos. Suaviza a sombra do key
    { glm::normalize(glm::vec3(-1.0f,  0.2f,  1.0f)), 3.5f, glm::vec3(0.85f, 0.9f, 1.0f), 0.8f, true },
    // 2 - BACK: atrás e acima. Desenha um contorno de luz e separa do fundo
    { glm::normalize(glm::vec3(-0.4f,  1.0f, -1.0f)), 3.0f, glm::vec3(1.0f, 1.0f, 1.0f),  1.0f, true },
};

vector<Object3D> cena;
int selecionado = 0;
char modo = 'T'; // 'T' -> translação, 'R' -> rotação, 'E' -> escala 
bool escalaUniforme = false;

// shaders

const GLchar *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texc;
layout (location = 2) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 texCoord;
out vec3 fragPos;
out vec3 scaledNormal;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    texCoord = texc;

    // posicao do fragmento no espaco do mundo, para calcular as direcoes da luz e da camera
    fragPos = vec3(model * vec4(position, 1.0));
   
    // matriz normal: ignora a translacao e corrige a escala nao uniforme
    scaledNormal = mat3(transpose(inverse(model))) * normal;
})";

const GLchar *fragmentShaderSource = R"(
#version 330 core
in vec2 texCoord;
in vec3 fragPos;
in vec3 scaledNormal;

uniform sampler2D texBuff;
uniform int isSelected;

uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float q;

#define N_LUZES 3

uniform vec3 lightPos[N_LUZES];
uniform vec3 lightColor[N_LUZES];
uniform float lightIntensity[N_LUZES];
uniform int lightOn[N_LUZES];

uniform float Ia;

uniform float Kc;
uniform float Kl;
uniform float Kq;

uniform vec3 cameraPos;

out vec4 color;

void main()
{
    vec4 texColor = texture(texBuff, texCoord);

    vec3 N = normalize(scaledNormal);
    vec3 V = normalize(cameraPos - fragPos);

    vec3 ambient = Ia * ka;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < N_LUZES; i++)
    {
        if (lightOn[i] == 0) continue;   // luz desligada

        vec3 L = lightPos[i] - fragPos;
        float d = length(L);
        L = normalize(L);

        float fatt = 1.0 / (Kc + Kl * d + Kq * d * d);
        float peso = fatt * lightIntensity[i];

        float diff = max(dot(N, L), 0.0);
        diffuse += peso * kd * diff * lightColor[i];

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, V), 0.0), q);
        specular += peso * ks * spec * lightColor[i];
    }

    // a textura faz o papel da cor do objeto, a especular fica de fora
    // porque o brilho tem a cor da luz, nao a do material
    vec3 result = (ambient + diffuse) * texColor.rgb + specular;

    if (isSelected == 0)
        result *= 0.85;
   
    color = vec4(result, texColor.a);
})";

// callbacks

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_P)
    {
        static bool wireframe = false;
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }

    // seleção
    if (key == GLFW_KEY_TAB)
    {
        selecionado = (selecionado + 1) % cena.size();
        cout << "Objeto selecionado: " << selecionado << endl;
    }

    // escolha do modo
    if (key == GLFW_KEY_R) { modo = 'R'; cout << "Modo: ROTACAO" << endl; }
    if (key == GLFW_KEY_T) { modo = 'T'; cout << "Modo: TRANSLACAO" << endl; }
    if (key == GLFW_KEY_E) { modo = 'E'; cout << "Modo: ESCALA" << endl; }

    if (key == GLFW_KEY_U)
    {
        escalaUniforme = !escalaUniforme;
        cout << "Escala uniforme: " << (escalaUniforme ? "ON" : "OFF") << endl;
    }

    // aplica no objeto selecionado
    Object3D &obj = cena[selecionado];

    // SHIFT inverte o sentido da transformação
    float sentido = (mode & GLFW_MOD_SHIFT) ? -1.0f : 1.0f;

    float passoRot = 5.0f * sentido;
    float passoTrans = 0.1f * sentido;
    float passoEscala = 1.0f + 0.05f * sentido;

    glm::vec3 eixo(0.0f);
    if (key == GLFW_KEY_X) eixo.x = 1.0f;
    if (key == GLFW_KEY_Y) eixo.y = 1.0f;
    if (key == GLFW_KEY_Z) eixo.z = 1.0f;

    if (eixo != glm::vec3(0.0f))
    {
        if (modo == 'R')
            obj.rotation += eixo * passoRot;
        else if (modo == 'T')
            obj.position += eixo * passoTrans;
        else if (modo == 'E')
        {
            if (escalaUniforme)
                obj.scale *= passoEscala;
            else
                obj.scale *= glm::mix(glm::vec3(1.0f), glm::vec3(passoEscala), eixo);
        }
    }

    // translação alternativa pelas setas (X e Y)
    if (modo == 'T')
    {
        if (key == GLFW_KEY_LEFT)  obj.position.x -= 0.1f;
        if (key == GLFW_KEY_RIGHT) obj.position.x += 0.1f;
        if (key == GLFW_KEY_UP)    obj.position.y += 0.1f;
        if (key == GLFW_KEY_DOWN)  obj.position.y -= 0.1f;
    }

    if (key == GLFW_KEY_SPACE)
    {
        for (size_t i = 0; i < cena.size(); i++)
        {
            cena[i].position = cena[i].posicaoInicial;
            cena[i].scale = glm::vec3(1.0f);
            cena[i].rotation = glm::vec3(0.0f);
        }
        cout << "Cena resetada" << endl;
    }

    if (key == GLFW_KEY_1 || key == GLFW_KEY_2 || key == GLFW_KEY_3)
    {
        int i = key - GLFW_KEY_1;   // 0, 1 ou 2
        luzes[i].ligada = !luzes[i].ligada;
        const char *nomes[3] = {"KEY (principal)", "FILL (preenchimento)", "BACK (fundo)"};
        cout << "Luz " << nomes[i] << ": " << (luzes[i].ligada ? "ON" : "OFF") << endl;
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    camera.processarMouse(xpos, ypos);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.zoom((float)yoffset);
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.mover(Camera::FRENTE, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.mover(Camera::TRAS, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.mover(Camera::ESQUERDA, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.mover(Camera::DIREITA, deltaTime);
}

// shader

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cerr << "ERRO no vertex shader:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cerr << "ERRO no fragment shader:\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cerr << "ERRO ao linkar o shader:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Lê os coeficientes de iluminação (Ka, Kd, Ks, Ns) e a textura de um arquivo .MTL
Material loadMTL(string filePath)
{
    Material mat;

    ifstream arq(filePath.c_str());

    if (!arq.is_open()) 
    { 
        cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
        return mat;
    }

    string line;
    while (getline(arq, line))    
    {
        istringstream ssline(line);
        string word;
        ssline >> word;            

        if (word == "map_Kd")         
        {
            ssline >> mat.textura;
        }
        else if (word=="Ka") 
        {
            ssline >> mat.ka.r >> mat.ka.g >> mat.ka.b;
        }
        else if (word=="Kd") 
        {
            ssline >> mat.kd.r >> mat.kd.g >> mat.kd.b;
        }
        else if (word=="Ks") 
        {
            ssline >> mat.ks.r >> mat.ks.g >> mat.ks.b;
        }
        else if (word=="Ns") 
        {
            ssline >> mat.ns;
        }
    }
    arq.close();
    return mat;
}

GLuint loadTexture(string filePath, int &width, int &height)
{
	GLuint texID; // id da textura a ser carregada

	// Gera o identificador da textura na memória
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	// Ajuste dos parâmetros de wrapping e filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Carregamento da imagem usando a função stbi_load da biblioteca stb_image
	int nrChannels;

	unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

	if (data)
	{
		if (nrChannels == 3) // jpg, bmp
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		else // assume que é 4 canais png
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture " << filePath << std::endl;
	}

	stbi_image_free(data);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texID;
}

/*
 * Le um arquivo .OBJ: monta o buffer de vértices (posição, coordenada de
 * textura e normal) e envia para a GPU. Lê o material pelo mtllib.
 *
 * Retorna o VAO, ou -1 se não conseguir abrir o arquivo.
 * nVertices e material são preenchidos por referência
 * (material.textura fica vazio se o modelo não tiver textura).
 */
int loadSimpleOBJ(string filePath, int &nVertices, Material &material)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat> vBuffer;
    string nomeMTL = "";

    ifstream arqEntrada(filePath.c_str());
    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
        return -1;
    }

    string line;
    while (getline(arqEntrada, line))
    {
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "v")
        {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        }
        else if (word == "vt")
        {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 vn;
            ssline >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word=="mtllib")
        {
            ssline >> nomeMTL;
        }
        else if (word == "f")
        {
            // guarda os índices (vértice, textura, normal) de cada vértice da face
            vector<glm::ivec3> faceIndices;

            while (ssline >> word)
            {
                int vi = 0;
                int ti = 0;
                int ni = 0;
                istringstream ss(word);
                string index;

                // cada getline avanca um campo do formato v/vt/vn
                // índice do vértice
                if (getline(ss, index, '/') && !index.empty())
                    vi = stoi(index) - 1; // o .OBJ indexa a partir de 1

                // índice da coordenada de textura
                if (getline(ss, index, '/') && !index.empty())
                    ti = stoi(index) - 1;

                // índice da normal
                if (getline(ss, index, '/') && !index.empty())
                    ni = stoi(index) - 1;

                if ((vi >= 0 && vi < (int)vertices.size()) && 
                    (ti >= 0 && ti < (int)texCoords.size()) && 
                    (ni >= 0 && ni < (int)normals.size()))
                {
                    faceIndices.push_back(glm::ivec3(vi, ti, ni));
                }
            }

            // triangulação em leque: cobre faces com mais de 3 vértices
            for (size_t i = 1; i + 1 < faceIndices.size(); i++)
            {
                glm::ivec3 tri[3] = {faceIndices[0], faceIndices[i], faceIndices[i + 1]};
                for (int k = 0; k < 3; k++)
                {
                    glm::vec3 v = vertices[tri[k].x];
                    glm::vec2 vt = texCoords[tri[k].y];
                    glm::vec3 vn = normals[tri[k].z];

                    vBuffer.push_back(v.x);
                    vBuffer.push_back(v.y);
                    vBuffer.push_back(v.z);
                    vBuffer.push_back(vt.s);
                    vBuffer.push_back(vt.t);
                    vBuffer.push_back(vn.x);
                    vBuffer.push_back(vn.y);
                    vBuffer.push_back(vn.z);
                }
            }
        }
    }

    arqEntrada.close();

    string diretorio = filePath.substr(0, filePath.find_last_of("/\\") + 1);

    cout << "Modelo carregado: " << filePath << endl;
    cout << "  vertices lidos (v): " << vertices.size() << endl;

    GLuint VBO, VAO;

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // atributo 0: posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    // atributo 1: coordenadas de textura (s,t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // atributo 2: vetor normal (nx, ny, nz)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 8; // x, y, z, s, t, nx, ny, nz
    cout << "  vertices no buffer: " << nVertices << " (" << nVertices / 3 << " triangulos)" << endl;
    cout << "  normais lidas (vn): " << normals.size() << endl;

    if (!nomeMTL.empty()) 
    {
        // o .obj aponta para o .mtl, que aponta para a imagem da textura
        string caminhoMTL = diretorio + nomeMTL;
        material = loadMTL(caminhoMTL);
       
        if (!material.textura.empty())
            material.textura = diretorio + material.textura;
    }
    
    return VAO;
}

/*  
    GLM faz a multiplicação pela direita, então a ordem que o vértice 
    é afetado é a inversa da escrita: scale -> rotate -> translate.
    a razão da ordem escolhida é porque a rotação e escalação 
    acontecem em torno da origem. se mudarmos a posição do objeto
    na tela, a origem é alterada. ex.: caso a ordem fosse alterada,
    se transladar primeiro, o objeto estaria longe da origem, 
    então ao rotacionar, ele orbitaria a "origem" ao invés de 
    girar sobre o próprio eixo.
*/

glm::mat4 getModelMatrix(const Object3D &obj)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, obj.scale);
    return model;
}

// main

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Exercício M5", nullptr, nullptr);
    if (!window)
    {
        cerr << "Falha ao criar a janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);  // esconde o cursor

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Falha ao inicializar o GLAD" << endl;
        return -1;
    }

    cout << "Placa de video: " << glGetString(GL_RENDERER) << endl;
    cout << "Versao do OpenGL: " << glGetString(GL_VERSION) << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint shaderID = setupShader();

    int nVertices = 0;
    Material material;
    GLuint suzanneVAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, material);

    cout << "  Ka: " << material.ka.r << " " << material.ka.g << " " << material.ka.b << endl;
    cout << "  Kd: " << material.kd.r << " " << material.kd.g << " " << material.kd.b << endl;
    cout << "  Ks: " << material.ks.r << " " << material.ks.g << " " << material.ks.b << endl;
    cout << "  Ns: " << material.ns << endl;

    GLuint texID = 0;
    if (!material.textura.empty())
    {
        int imgW, imgH;
        // o PNG é lido de cima para baixo, mas o OpenGL espera a origem
        // da textura embaixo a esquerda (sem isso a textura fica invertida)
        stbi_set_flip_vertically_on_load(true);
        texID = loadTexture(material.textura, imgW, imgH);
    }

    Object3D obj1;
    obj1.VAO = suzanneVAO;
    obj1.nVertices = nVertices;
    obj1.position = glm::vec3(-1.8f, 0.0f, 0.0f);
    obj1.posicaoInicial = glm::vec3(-1.8f, 0.0f, 0.0f);
    obj1.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    obj1.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    cena.push_back(obj1);

    Object3D obj2 = obj1;                          // mesma geometria
    obj2.position = glm::vec3(1.8f, 0.0f, 0.0f);   // outra posição
    obj2.posicaoInicial = glm::vec3(1.8f, 0.0f, 0.0f);
    cena.push_back(obj2);

    glUseProgram(shaderID);

    // o shader recebe o numero da unidade de textura (0), nao o id da textura
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint selectedLoc = glGetUniformLocation(shaderID, "isSelected");

    // material (lido do .mtl)
    glUniform3fv(glGetUniformLocation(shaderID, "ka"), 1, glm::value_ptr(material.ka));
    glUniform3fv(glGetUniformLocation(shaderID, "kd"), 1, glm::value_ptr(material.kd));
    glUniform3fv(glGetUniformLocation(shaderID, "ks"), 1, glm::value_ptr(material.ks));
    
    // q (Ns no .mtl) controla o quanto o reflexo pode se desviar da câmera
    // e ainda gerar brilho. Para testar, trocar material.ns por um valor fixo:
    // q = 5.0f   ==> tolera muito desvio: brilho grande e espalhado (superfície fosca)
    // q = 233.0f ==> tolera pouco desvio: brilho pequeno e concentrado (plástico, verniz)
    glUniform1f(glGetUniformLocation(shaderID, "q"), material.ns);

    glUniform1f(glGetUniformLocation(shaderID, "Ia"), 0.2f);
    glUniform1f(glGetUniformLocation(shaderID, "Kc"), 1.0f);
    glUniform1f(glGetUniformLocation(shaderID, "Kl"), 0.14f);
    glUniform1f(glGetUniformLocation(shaderID, "Kq"), 0.07f);


    glEnable(GL_DEPTH_TEST);

    // loop principal
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        processInput(window);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix((float)WIDTH / (float)HEIGHT);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderID, "cameraPos"), 1, glm::value_ptr(camera.position));

        glClearColor(0.97f, 0.89f, 0.87f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_2D, texID);

        // as 3 luzes se posicionam a partir do objeto principal (o primeiro da cena)
        const Object3D &principal = cena[0];
        float raio = glm::max(glm::max(principal.scale.x, principal.scale.y), principal.scale.z);

        for (int i = 0; i < 3; i++)
        {
            glm::vec3 pos = principal.position + luzes[i].direcao * (luzes[i].distancia * raio);

            string idx = "[" + to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaderID, ("lightPos" + idx).c_str()), 1, glm::value_ptr(pos));
            glUniform3fv(glGetUniformLocation(shaderID, ("lightColor" + idx).c_str()), 1, glm::value_ptr(luzes[i].cor));
            glUniform1f(glGetUniformLocation(shaderID, ("lightIntensity" + idx).c_str()), luzes[i].intensidade);
            glUniform1i(glGetUniformLocation(shaderID, ("lightOn" + idx).c_str()), luzes[i].ligada ? 1 : 0);
        }

        // desenha cada objeto da cena com sua própria matriz de modelo
        for (size_t i = 0; i < cena.size(); i++)
        {
            glUniform1i(selectedLoc, (int)i == selecionado ? 1 : 0);

            glm::mat4 model = getModelMatrix(cena[i]);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glBindVertexArray(cena[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, cena[i].nVertices);
        }
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &suzanneVAO);
    glfwTerminate();
    return 0;
}