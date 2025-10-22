# Algoritmos e Estruturas de Dados II  
## Trabalho I — Arquivos de Dados com Índices Parciais

---

### Objetivo
Criação de arquivos de dados e de arquivos de índice parcial para uma **organização sequencial-indexado** .

---

### Organização
- Trabalho **individual** ou em **dupla**, se o trabalho for em dupla, definir seu grupo para a dupla no AVA.
- Crie um **repositório no GitHub** contendo:
  - Contexto e definição do problema  
  - Perguntas (consultas)  
  - Implementação de código
  - Arquivos de dados e índices  
  - Documentação complementar

---

## Contexto do Trabalho

Arquivos de dados ou  datasets são arquivos definidos e estruturados como  **parte de uma organização de arquivos** que serão utilizados para consultas ou para alteração do conjunto de dados. Grandes volumes de dados são gerados a cada dia, e esses dados são de alguma forma guardados em arquivos, muitas vezes arquivos com grandes volumes de dados.

Conhecendo como um arquivo está organizado internamente, pode-se desenvolver programas ou procedimentos   para   consultar   algum   tipo   de   informação.   Cada   consulta  é  realizada   para responder a uma pergunta específica:

#### Exemplo
> “Qual é a joia mais vendida em um site de uma loja online de joias de médio porte?”

Para isso, é possível:
- Criar uma base de dados a partir de extrações de dados do _e-commerce_, **ou**
- Utilizar datasets abertos, como arquivos CSV públicos, gerando um arquivo de dados inico que contenha as informações que serão pesquisadas depois.

#### Hipóteses possíveis
- Pode existir um tipo de joia que seja líder absoluta de vendas (>50% das compras).  
- Pode haver equilíbrio de vendas entre categorias diferentes.

O próximo passo é **extrair as informações** e **montar uma base de dados**, definindo quais informações serão incluídas e de onde elas serão obtidas.

---

##  Atividades a Realizar

### 1. Definição do contexto a ser explorado

O dataset contém **dados de compras de dezembro/2018 a dezembro/2021** (3 anos) de uma loja online de joias.  
Cada linha representa um produto comprado; vários produtos do mesmo pedido compartilham o campo `order_id`.

📦 Dataset:  
[Histórico de compras de joias de um Ecommerce](https://www.kaggle.com/datasets/mkechinov/ecommerce-purchase-history-from-jewelry-store/data)

---

### 2. Montagem dos Arquivos de Dados

A primeira atividade do trabalho envolve a construção dos arquivos de dados. O dataset a ser utilizado tem dados de compras em uma loja online realizadas durante 3 anos, e contém:

| Campo | Descrição |
|--------|------------|
| `datetime` | Data e hora da compra |
| `order_id` | ID do pedido |
| `product_id` | ID do produto adquirido |
| `quantity` | Quantidade (SKU¹) |
| `category_id` | ID da categoria |
| `category_alias` | Nome/alias da categoria |
| `brand_id` | ID da Marca |
| `price_usd` | Preço (USD) |
| `user_id` | ID do usuário |
| `product_gender` | Gênero do produto |

¹ Stock Keeping Unit, ou Unidade de Manutenção de Estoque, e é um código alfanumérico interno que uma empresa cria para identificar de forma única cada variação de um produto em seu estoque, como cor, tamanho ou modelo

Você deve criar **dois arquivos de dados**:
- `cadastro.dat` → arquivo de joias 
- `pedidos.dat` → arquivo de acesso compras 

Como a organização de arquivos de dados definida é sequencial, os arquivos devem estar ordenados por algum dos campos, preferencialmente o campo com um identificador (campo chave). Assim, as seguintes tarefas deverão ser realizadas:

#### Requisitos
- Escolher os arquivos que serão criados com o dataset fornecido
- Cada arquivo deve ter no **mínimo 3 campos**:
  - Um **campo chave (não repetido)**  
  - Um ou mais **campos repetidos**
- Definir **2 ou 3 consultas** simples que podem ser respondidas pelo conjunto de dados.  
- Ordenar os dados pelo **campo chave** (utilizando algum método de ordenação estudado).  
- Criar os arquivos em **modo binário** (não textual).

---

### 2.1 Organização e Registros do Arquivo de Dados

Os registros devem possuir **tamanho fixo**.  
Preencha com **espaços em branco** ao final de campos textuais, garantindo comprimento uniforme.  
Cada linha termina com o caractere `\n`.

A implementação deve utilizar uma linguagem de programação (`C`, `C#`, `C++`, `Python`, `PHP`, `Java`, ...). que possua o comando **`seek`** ou equivalente  


#### Funções obrigatórias para cada arquivo de dados
1. Inserir dados
→ explicar como os dados foram ordenados e gravados

2. Mostrar dados
→ leitura e exibição dos registros

3. Pesquisa binária
→ localizar registros pelo campo chave

4. Consulta de dados
→ ler e retornar informações a partir da pesquisa binária

No final, crie dois índices parciais (um por arquivo).
Eles devem ser salvos em disco e carregados automaticamente ao abrir o programa.

---
### 2.2 Índices em Arquivo

* Implemente **um arquivo de índice parcial** para o campo **chave** de cada arquivo de dados de acordo com a descrição do índice de arquivo da organização sequencial-indexado;

* **Implemente uma função de consulta a partir deste índice** usando a **pesquisa binária** para pesquisar no arquivo de índice e, depois o comando seek para pesquisar no arquivo de dados

---

## 3. Inserção e Remoção de Dados

Implemente as operações de inserção e remoção de registros em um dos arquivos.
Essas operações devem provocar reconstrução do índice correspondente.
Defina se a reconstrução:
* Acontece a cada modificação, ou
* Segue outro critério (ex: reconstruir apenas após N alterações).

Perguntas a responder
* Como um novo registro é inserido?
* Como um registro é removido?
* O índice é atualizado automaticamente ou sob demanda?

## 4. Postagem no AVA

Publique no AVA:
* Descrição dos arquivos de dados
* Descrição dos arquivos de índices
* Link do repositório GitHub contendo:
    * Código-fonte da implementação
    * Arquivos .dat
    * Arquivos de índice .idx

## Regras

Não é permitido armazenar todos os registros em memória RAM.
Somente os dados necessários podem ser carregados.
Todas as operações devem ocorrer com arquivos na memória secundária.

📚 Referências e Links

Dataset base:
https://www.kaggle.com/datasets/mkechinov/ecommerce-purchase-history-from-jewelry-store/data
