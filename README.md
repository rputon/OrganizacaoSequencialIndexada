# Algoritmos e Estruturas de Dados II  
## Trabalho I — Arquivos de Dados com Índices Parciais

---
### Contexto e definição do problema
Arquivos de dados ou  datasets são arquivos definidos e estruturados como  **parte de uma organização de arquivos** que serão utilizados para consultas ou para alteração do conjunto de dados. Grandes volumes de dados são gerados a cada dia, e esses dados são de alguma forma guardados em arquivos, muitas vezes arquivos com grandes volumes de dados.

O dataset contém **dados de compras de dezembro/2018 a dezembro/2021** (3 anos) de uma loja online de joias.  
Cada linha representa um produto comprado; vários produtos do mesmo pedido compartilham o campo `order_id`.

Como a organização de arquivos de dados definida é sequencial, os arquivos devem estar ordenados por algum dos campos, preferencialmente o campo com um identificador (campo chave).

- Escolher os arquivos que serão criados com o dataset fornecido
- Cada arquivo deve ter no **mínimo 3 campos**:
  - Um **campo chave (não repetido)**  
  - Um ou mais **campos repetidos**
- Definir **2 ou 3 consultas** simples que podem ser respondidas pelo conjunto de dados.  
- Ordenar os dados pelo **campo chave** (utilizando algum método de ordenação estudado).  
- Criar os arquivos em **modo binário** (não textual).

---
### Perguntas (consultas)
    Qual é a joia mais vendida em um site de uma loja online de joias de médio porte?

    Qual o mês em que mais se vende joias?

    Qual a categoria de joias que mais vende?

---
### Arquivos de dados e índices
 **jewelry.csv**: Arquivo de dados original com todos os registros de vendas
 
 **orderHistory.dat**: Arquivo binário para armazenar o histórico de vendas (jewelry.csv)

 **orderIndex.idx**: Arquivo binário para armazenar o índice sequencial do arquivo *orderHistory.dat*

 **jewelryRegister.dat**: Arquivo binário contendo o registro de todas as joias vendidas

 **jewelryIndex.idx**: Arquivo binário para armazenar o índice sequencial do arquivo *jewelryRegister.dat*

 **categoryRegister.dat**: Arquivo binário contendo o registro de todas as categorias de joias vendidas

 **categoryIndex.idx**: Arquivo binário armazendo o índice sequencial do arquivo *categoryRegister.dat*

 **orderOverflow.dat**: Arquivo binário que armazena as ordens adicionadas e que deram overflow nos blocos do índice


