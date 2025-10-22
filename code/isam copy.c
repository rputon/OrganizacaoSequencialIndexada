#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#define MEMORY_LIMIT 10000
#define HASH_SIZE 50000
#define REMOVED_FLAG '*'
#define REBUILD_THRESHOLD 10
#define BLOCK_SIZE 100

int removal_count = 0;

typedef struct{
    char data[30];
    long long int order_id;
    long long int product_id;
    int quantity;
    long long int category_id;
    char category_alias[30];
    int brand_id;
    float price_usd;
    long long int user_id;
    char product_gender;
    char color[10];
    char metal[10];
    char gem[25];
} ORDER;

typedef struct{
    long long int product_id;
    long long int category_id;
    int brand_id;
    float price_usd;
    char product_gender;
    char color[10];
    char metal[10];
    char gem[25];
} JEWELRY;

typedef struct{
    long long int id;
    long position;
} INDEX;

typedef struct {
    long long int category_id;
    char category_alias[30];
    int product_count;      // Quantidade de produtos únicos nesta categoria
    int total_sales;        // Quantidade total de itens vendidos
    float total_revenue;    // Receita total
} CATEGORY;

typedef struct {
    long long int product_id;
    int total_quantity;
} PRODUCT_SALES;

typedef struct HashNode {
    long long int product_id;
    int total_quantity;
    struct HashNode* next;
} HashNode;

typedef struct {
    int year;
    int month;
    int total_orders;
    int total_quantity;
    float total_revenue;
} MONTHLY_SALES;

typedef struct {
    ORDER record;
    long originalBlockPos;
    long nextOverflow;
} OVERFLOW_RECORD;

// Estrutura para agregação temporária de categorias
typedef struct CategoryNode {
    long long int category_id;
    char category_alias[30];
    int product_count;
    int total_sales;
    float total_revenue;
    struct CategoryNode* next;
} CategoryNode;

FILE* openFile(const char *fileName, const char *mode){
    FILE* file = fopen(fileName, mode);
    if(file == NULL){
        printf("Erro ao abrir o arquivo %s.\n", fileName);
        return NULL;
    }
    return file;
}

int compareOrders(const void* a, const void* b) {
    ORDER* orderA = (ORDER*)a;
    ORDER* orderB = (ORDER*)b;
    if (orderA->order_id < orderB->order_id) return -1;
    if (orderA->order_id > orderB->order_id) return 1;
    return 0;
}

int compareJewelry(const void* a, const void* b) {
    JEWELRY* jewA = (JEWELRY*)a;
    JEWELRY* jewB = (JEWELRY*)b;
    if (jewA->product_id < jewB->product_id) return -1;
    if (jewA->product_id > jewB->product_id) return 1;
    return 0;
}

int compareCategory(const void* a, const void* b) {
    CATEGORY* catA = (CATEGORY*)a;
    CATEGORY* catB = (CATEGORY*)b;
    if (catA->category_id < catB->category_id) return -1;
    if (catA->category_id > catB->category_id) return 1;
    return 0;
}

int compareCategorySales(const void* a, const void* b) {
    CATEGORY* catA = (CATEGORY*)a;
    CATEGORY* catB = (CATEGORY*)b;
    // Ordem decrescente por total_sales
    if (catA->total_sales > catB->total_sales) return -1;
    if (catA->total_sales < catB->total_sales) return 1;
    return 0;
}

int parseCSVLine(char* line, ORDER* order) {
    char *ptr = line;
    int field = 0;
    char buffer[256];
    int bufIdx = 0;
    
    while (*ptr) {
        if (*ptr == ',' || *ptr == '\n' || *ptr == '\r') {
            buffer[bufIdx] = '\0';
            
            switch(field) {
                case 0: strncpy(order->data, buffer, sizeof(order->data)-1); break;
                case 1: order->order_id = atoll(buffer); break;
                case 2: order->product_id = atoll(buffer); break;
                case 3: order->quantity = atoi(buffer); break;
                case 4: order->category_id = atoll(buffer); break;
                case 5: strncpy(order->category_alias, buffer, sizeof(order->category_alias)-1); break;
                case 6: order->brand_id = atoi(buffer); break;
                case 7: order->price_usd = atof(buffer); break;
                case 8: order->user_id = atoll(buffer); break;
                case 9: order->product_gender = buffer[0]; break;
                case 10: strncpy(order->color, buffer, sizeof(order->color)-1); break;
                case 11: strncpy(order->metal, buffer, sizeof(order->metal)-1); break;
                case 12: strncpy(order->gem, buffer, sizeof(order->gem)-1); break;
            }
            
            field++;
            bufIdx = 0;
            ptr++;
            continue;
        }
        buffer[bufIdx++] = *ptr++;
    }
    
    return field > 0;
}

// FASE 1: Cria runs ordenados para ORDERS, JEWELRY e CATEGORY
int createSortedRuns(FILE* csv, int* numOrderRuns, int* numJewelryRuns, int* numCategoryRuns) {
    char line[200];
    ORDER* orderBuffer = malloc(MEMORY_LIMIT * sizeof(ORDER));
    JEWELRY* jewelryBuffer = malloc(MEMORY_LIMIT * sizeof(JEWELRY));
    
    // Hash table temporária para categorias (evita duplicatas durante a leitura)
    CategoryNode** categoryHash = calloc(1000, sizeof(CategoryNode*));
    
    int orderCount = 0;
    int jewelryCount = 0;
    int orderRunNum = 0;
    int jewelryRunNum = 0;
    
    printf("Fase 1: Criando runs ordenados...\n");
    
    fgets(line, sizeof(line), csv);  // Pula cabeçalho
    
    while (fgets(line, sizeof(line), csv) != NULL) {
        ORDER order;
        if (!parseCSVLine(line, &order)) continue;
        
        // Adiciona ORDER ao buffer
        orderBuffer[orderCount++] = order;
        
        // Cria JEWELRY
        JEWELRY jewelry;
        jewelry.product_id = order.product_id;
        jewelry.category_id = order.category_id;
        jewelry.brand_id = order.brand_id;
        jewelry.price_usd = order.price_usd;
        jewelry.product_gender = order.product_gender;
        strncpy(jewelry.color, order.color, sizeof(jewelry.color)-1);
        strncpy(jewelry.metal, order.metal, sizeof(jewelry.metal)-1);
        strncpy(jewelry.gem, order.gem, sizeof(jewelry.gem)-1);
        
        jewelryBuffer[jewelryCount++] = jewelry;
        
        // Agrega dados de CATEGORIA
        int hashIdx = order.category_id % 1000;
        CategoryNode* current = categoryHash[hashIdx];
        CategoryNode* found = NULL;
        
        while (current) {
            if (current->category_id == order.category_id) {
                found = current;
                break;
            }
            current = current->next;
        }
        
        if (found) {
            found->total_sales += order.quantity;
            found->total_revenue += (order.price_usd * order.quantity);
        } else {
            CategoryNode* newNode = malloc(sizeof(CategoryNode));
            newNode->category_id = order.category_id;
            strncpy(newNode->category_alias, order.category_alias, sizeof(newNode->category_alias)-1);
            newNode->product_count = 0;  // Será calculado depois
            newNode->total_sales = order.quantity;
            newNode->total_revenue = (order.price_usd * order.quantity);
            newNode->next = categoryHash[hashIdx];
            categoryHash[hashIdx] = newNode;
        }
        
        // Quando buffers enchem, ordena e salva
        if (orderCount >= MEMORY_LIMIT) {
            // Salva ORDER run
            qsort(orderBuffer, orderCount, sizeof(ORDER), compareOrders);
            char filename[50];
            sprintf(filename, "../data/temp_order_run_%d.dat", orderRunNum);
            FILE* f = fopen(filename, "wb");
            fwrite(orderBuffer, sizeof(ORDER), orderCount, f);
            fclose(f);
            orderRunNum++;
            orderCount = 0;
            
            // Salva JEWELRY run
            qsort(jewelryBuffer, jewelryCount, sizeof(JEWELRY), compareJewelry);
            sprintf(filename, "../data/temp_jewelry_run_%d.dat", jewelryRunNum);
            f = fopen(filename, "wb");
            fwrite(jewelryBuffer, sizeof(JEWELRY), jewelryCount, f);
            fclose(f);
            jewelryRunNum++;
            jewelryCount = 0;
            
            printf("  Runs %d criados\n", orderRunNum);
        }
    }
    
    // Processa últimos chunks
    if (orderCount > 0) {
        qsort(orderBuffer, orderCount, sizeof(ORDER), compareOrders);
        char filename[50];
        sprintf(filename, "../data/temp_order_run_%d.dat", orderRunNum);
        FILE* f = fopen(filename, "wb");
        fwrite(orderBuffer, sizeof(ORDER), orderCount, f);
        fclose(f);
        orderRunNum++;
    }
    
    if (jewelryCount > 0) {
        qsort(jewelryBuffer, jewelryCount, sizeof(JEWELRY), compareJewelry);
        char filename[50];
        sprintf(filename, "../data/temp_jewelry_run_%d.dat", jewelryRunNum);
        FILE* f = fopen(filename, "wb");
        fwrite(jewelryBuffer, sizeof(JEWELRY), jewelryCount, f);
        fclose(f);
        jewelryRunNum++;
    }
    
    // Salva CATEGORIAS em um único run ordenado
    int categoryCount = 0;
    CATEGORY* categoryBuffer = malloc(1000 * sizeof(CATEGORY));
    
    for (int i = 0; i < 1000; i++) {
        CategoryNode* current = categoryHash[i];
        while (current) {
            categoryBuffer[categoryCount].category_id = current->category_id;
            strncpy(categoryBuffer[categoryCount].category_alias, current->category_alias, 
                    sizeof(categoryBuffer[categoryCount].category_alias)-1);
            categoryBuffer[categoryCount].product_count = 0;  // Será calculado na próxima etapa
            categoryBuffer[categoryCount].total_sales = current->total_sales;
            categoryBuffer[categoryCount].total_revenue = current->total_revenue;
            categoryCount++;
            
            CategoryNode* temp = current;
            current = current->next;
            free(temp);
        }
    }
    
    if (categoryCount > 0) {
        qsort(categoryBuffer, categoryCount, sizeof(CATEGORY), compareCategory);
        FILE* f = fopen("../data/temp_category_run_0.dat", "wb");
        fwrite(categoryBuffer, sizeof(CATEGORY), categoryCount, f);
        fclose(f);
        *numCategoryRuns = 1;
    } else {
        *numCategoryRuns = 0;
    }
    
    free(orderBuffer);
    free(jewelryBuffer);
    free(categoryBuffer);
    free(categoryHash);
    
    *numOrderRuns = orderRunNum;
    *numJewelryRuns = jewelryRunNum;
    
    printf("Total: %d order runs, %d jewelry runs, %d category runs\n\n", 
           orderRunNum, jewelryRunNum, *numCategoryRuns);
    return orderRunNum;
}

// FASE 2A: Mescla runs de ORDERS com índice
int mergeOrderRuns(int numRuns, FILE* orderHistory, FILE* orderIndex, int indexGap) {
    printf("Fase 2A: Mesclando ORDER runs...\n");
    
    FILE** runFiles = malloc(numRuns * sizeof(FILE*));
    ORDER* currentOrders = malloc(numRuns * sizeof(ORDER));
    int* runFinished = calloc(numRuns, sizeof(int));
    
    for (int i = 0; i < numRuns; i++) {
        char filename[50];
        sprintf(filename, "../data/temp_order_run_%d.dat", i);
        runFiles[i] = fopen(filename, "rb");
        if (fread(&currentOrders[i], sizeof(ORDER), 1, runFiles[i]) != 1) {
            runFinished[i] = 1;
        }
    }
    
    const int WRITE_BUFFER_SIZE = 5000;
    ORDER* writeBuffer = malloc(WRITE_BUFFER_SIZE * sizeof(ORDER));
    int writeCount = 0;
    long totalWritten = 0;
    int indexCount = 0;
    
    while (1) {
        int minIndex = -1;
        long long minOrderId = LLONG_MAX;
        
        for (int i = 0; i < numRuns; i++) {
            if (!runFinished[i] && currentOrders[i].order_id < minOrderId) {
                minOrderId = currentOrders[i].order_id;
                minIndex = i;
            }
        }
        
        if (minIndex == -1) break;
        
        writeBuffer[writeCount++] = currentOrders[minIndex];
        
        if (totalWritten % indexGap == 0) {
            INDEX indexEntry = {currentOrders[minIndex].order_id, totalWritten * sizeof(ORDER)};
            fwrite(&indexEntry, sizeof(INDEX), 1, orderIndex);
            indexCount++;
        }
        
        totalWritten++;
        
        if (writeCount >= WRITE_BUFFER_SIZE) {
            fwrite(writeBuffer, sizeof(ORDER), writeCount, orderHistory);
            writeCount = 0;
        }
        
        if (fread(&currentOrders[minIndex], sizeof(ORDER), 1, runFiles[minIndex]) != 1) {
            runFinished[minIndex] = 1;
        }
    }
    
    if (writeCount > 0) {
        fwrite(writeBuffer, sizeof(ORDER), writeCount, orderHistory);
    }
    
    for (int i = 0; i < numRuns; i++) {
        fclose(runFiles[i]);
        char filename[50];
        sprintf(filename, "../data/temp_order_run_%d.dat", i);
        remove(filename);
    }
    
    free(runFiles);
    free(currentOrders);
    free(runFinished);
    free(writeBuffer);
    fflush(orderHistory);
    fflush(orderIndex);
    
    printf("Orders: %ld registros, %d indices\n\n", totalWritten, indexCount);
    return totalWritten;
}

// FASE 2B: Mescla runs de JEWELRY com índice
int mergeJewelryRuns(int numRuns, FILE* jewelryRegister, FILE* jewelryIndex, int indexGap) {
    printf("Fase 2B: Mesclando JEWELRY runs...\n");
    
    FILE** runFiles = malloc(numRuns * sizeof(FILE*));
    JEWELRY* currentJewelry = malloc(numRuns * sizeof(JEWELRY));
    int* runFinished = calloc(numRuns, sizeof(int));
    
    for (int i = 0; i < numRuns; i++) {
        char filename[50];
        sprintf(filename, "../data/temp_jewelry_run_%d.dat", i);
        runFiles[i] = fopen(filename, "rb");
        if (fread(&currentJewelry[i], sizeof(JEWELRY), 1, runFiles[i]) != 1) {
            runFinished[i] = 1;
        }
    }
    
    const int WRITE_BUFFER_SIZE = 5000;
    JEWELRY* writeBuffer = malloc(WRITE_BUFFER_SIZE * sizeof(JEWELRY));
    int writeCount = 0;
    long totalWritten = 0;
    int indexCount = 0;
    long long lastProductId = -1;
    
    while (1) {
        int minIndex = -1;
        long long minProductId = LLONG_MAX;
        
        for (int i = 0; i < numRuns; i++) {
            if (!runFinished[i] && currentJewelry[i].product_id < minProductId) {
                minProductId = currentJewelry[i].product_id;
                minIndex = i;
            }
        }
        
        if (minIndex == -1) break;
        
        if (currentJewelry[minIndex].product_id != lastProductId) {
            writeBuffer[writeCount++] = currentJewelry[minIndex];
            lastProductId = currentJewelry[minIndex].product_id;
            
            if (totalWritten % indexGap == 0) {
                INDEX indexEntry = {currentJewelry[minIndex].product_id, totalWritten * sizeof(JEWELRY)};
                fwrite(&indexEntry, sizeof(INDEX), 1, jewelryIndex);
                indexCount++;
            }
            
            totalWritten++;
            
            if (writeCount >= WRITE_BUFFER_SIZE) {
                fwrite(writeBuffer, sizeof(JEWELRY), writeCount, jewelryRegister);
                writeCount = 0;
            }
        }
        
        if (fread(&currentJewelry[minIndex], sizeof(JEWELRY), 1, runFiles[minIndex]) != 1) {
            runFinished[minIndex] = 1;
        }
    }
    
    if (writeCount > 0) {
        fwrite(writeBuffer, sizeof(JEWELRY), writeCount, jewelryRegister);
    }
    
    for (int i = 0; i < numRuns; i++) {
        fclose(runFiles[i]);
        char filename[50];
        sprintf(filename, "../data/temp_jewelry_run_%d.dat", i);
        remove(filename);
    }
    
    free(runFiles);
    free(currentJewelry);
    free(runFinished);
    free(writeBuffer);
    fflush(jewelryRegister);
    fflush(jewelryIndex);
    
    printf("Jewelry: %ld registros unicos, %d indices\n\n", totalWritten, indexCount);
    return totalWritten;
}

// FASE 2C: Processa CATEGORY - calcula product_count e cria índice
int processCategoryData(FILE* jewelryRegister, FILE* categoryRegister, 
                       FILE* categoryIndex, int indexGap) {
    printf("Fase 2C: Processando CATEGORY...\n");
    
    // Lê categorias do run temporário
    FILE* tempCat = fopen("../data/temp_category_run_0.dat", "rb");
    if (!tempCat) {
        printf("Erro ao abrir arquivo temporario de categorias\n");
        return 0;
    }
    
    fseek(tempCat, 0, SEEK_END);
    long catCount = ftell(tempCat) / sizeof(CATEGORY);
    fseek(tempCat, 0, SEEK_SET);
    
    CATEGORY* categories = malloc(catCount * sizeof(CATEGORY));
    fread(categories, sizeof(CATEGORY), catCount, tempCat);
    fclose(tempCat);
    remove("../data/temp_category_run_0.dat");
    
    // Conta produtos por categoria
    printf("Contando produtos por categoria...\n");
    
    fseek(jewelryRegister, 0, SEEK_END);
    long jewelryCount = ftell(jewelryRegister) / sizeof(JEWELRY);
    fseek(jewelryRegister, 0, SEEK_SET);
    
    JEWELRY jewelry;
    for (long i = 0; i < jewelryCount; i++) {
        if (fread(&jewelry, sizeof(JEWELRY), 1, jewelryRegister) != 1) break;
        
        // Encontra categoria e incrementa contador
        for (int j = 0; j < catCount; j++) {
            if (categories[j].category_id == jewelry.category_id) {
                categories[j].product_count++;
                break;
            }
        }
        
        if ((i + 1) % 1000 == 0) {
            printf("  %ld produtos processados...\r", i + 1);
            fflush(stdout);
        }
    }
    
    printf("\n");
    
    // Escreve categorias e cria índice
    for (int i = 0; i < catCount; i++) {
        long pos = i * sizeof(CATEGORY);
        fwrite(&categories[i], sizeof(CATEGORY), 1, categoryRegister);
        
        if (i % indexGap == 0) {
            INDEX indexEntry = {categories[i].category_id, pos};
            fwrite(&indexEntry, sizeof(INDEX), 1, categoryIndex);
        }
    }
    
    fflush(categoryRegister);
    fflush(categoryIndex);
    
    printf("Categories: %ld registros\n\n", catCount);
    
    free(categories);
    return catCount;
}

void readCSVExternalSort(FILE* csv, FILE* orderHistory, FILE* orderIndex, 
                        FILE* jewelryRegister, FILE* jewelryIndex,
                        FILE* categoryRegister, FILE* categoryIndex, int indexGap) {
    
    printf("=== EXTERNAL SORT COM ISAM ===\n");
    printf("Limite memoria: %d registros\n", MEMORY_LIMIT);
    printf("Gap indice: %d\n\n", indexGap);
    
    int numOrderRuns, numJewelryRuns, numCategoryRuns;
    createSortedRuns(csv, &numOrderRuns, &numJewelryRuns, &numCategoryRuns);
    mergeOrderRuns(numOrderRuns, orderHistory, orderIndex, indexGap);
    mergeJewelryRuns(numJewelryRuns, jewelryRegister, jewelryIndex, indexGap);
    processCategoryData(jewelryRegister, categoryRegister, categoryIndex, indexGap);
    
    printf("=== CARGA COMPLETA ===\n\n");
}

// Busca categoria por ID
CATEGORY* searchCategoryById(FILE* categoryRegister, FILE* categoryIndex, 
                             long long int category_id, int indexGap) {
    
    fseek(categoryIndex, 0, SEEK_END);
    long fileSize = ftell(categoryIndex);
    int totalEntries = fileSize / sizeof(INDEX);
    
    if (totalEntries == 0) return NULL;
    
    int left = 0, right = totalEntries - 1;
    long startPosition = 0;
    INDEX currentIndex;
    
    while (left <= right) {
        int middle = left + (right - left) / 2;
        fseek(categoryIndex, middle * sizeof(INDEX), SEEK_SET);
        fread(&currentIndex, sizeof(INDEX), 1, categoryIndex);
        
        if (currentIndex.id == category_id) {
            startPosition = currentIndex.position;
            break;
        }
        
        if (currentIndex.id < category_id) {
            startPosition = currentIndex.position;
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }
    
    CATEGORY* category = malloc(sizeof(CATEGORY));
    if (!category) return NULL;
    
    fseek(categoryRegister, startPosition, SEEK_SET);
    
    for (int i = 0; i < indexGap; i++) {
        if (fread(category, sizeof(CATEGORY), 1, categoryRegister) != 1) break;
        
        if (category->category_id == category_id) return category;
        if (category->category_id > category_id) break;
    }
    
    free(category);
    return NULL;
}

// Atualiza dados de vendas de uma categoria
int updateCategorySales(FILE* categoryRegister, FILE* categoryIndex, 
                       long long int category_id, int quantity, float revenue, int indexGap) {
    
    CATEGORY* cat = searchCategoryById(categoryRegister, categoryIndex, category_id, indexGap);
    
    if (!cat) {
        printf("Categoria %lld nao encontrada\n", category_id);
        return 0;
    }
    
    cat->total_sales += quantity;
    cat->total_revenue += revenue;
    
    // Encontra posição no arquivo e atualiza
    fseek(categoryRegister, 0, SEEK_SET);
    CATEGORY temp;
    long pos = 0;
    
    while (fread(&temp, sizeof(CATEGORY), 1, categoryRegister) == 1) {
        if (temp.category_id == category_id) {
            fseek(categoryRegister, pos, SEEK_SET);
            fwrite(cat, sizeof(CATEGORY), 1, categoryRegister);
            fflush(categoryRegister);
            free(cat);
            return 1;
        }
        pos += sizeof(CATEGORY);
    }
    
    free(cat);
    return 0;
}

// Reconstrói categorias a partir dos pedidos
void rebuildCategoryData(FILE* orderHistory, FILE* categoryRegister, 
                        FILE* categoryIndex, int indexGap) {
    printf("\n=== RECONSTRUINDO DADOS DE CATEGORIAS ===\n");
    
    // Hash table para agregação
    CategoryNode** categoryHash = calloc(1000, sizeof(CategoryNode*));
    
    fseek(orderHistory, 0, SEEK_END);
    long totalOrders = ftell(orderHistory) / sizeof(ORDER);
    fseek(orderHistory, 0, SEEK_SET);
    
    printf("Processando %ld pedidos...\n", totalOrders);
    
    ORDER order;
    for (long i = 0; i < totalOrders; i++) {
        if (fread(&order, sizeof(ORDER), 1, orderHistory) != 1) break;
        
        int hashIdx = order.category_id % 1000;
        CategoryNode* current = categoryHash[hashIdx];
        CategoryNode* found = NULL;
        
        while (current) {
            if (current->category_id == order.category_id) {
                found = current;
                break;
            }
            current = current->next;
        }
        
        if (found) {
            found->total_sales += order.quantity;
            found->total_revenue += (order.price_usd * order.quantity);
        } else {
            CategoryNode* newNode = malloc(sizeof(CategoryNode));
            newNode->category_id = order.category_id;
            strncpy(newNode->category_alias, order.category_alias, sizeof(newNode->category_alias)-1);
            newNode->product_count = 0;
            newNode->total_sales = order.quantity;
            newNode->total_revenue = (order.price_usd * order.quantity);
            newNode->next = categoryHash[hashIdx];
            categoryHash[hashIdx] = newNode;
        }
        
        if ((i + 1) % 10000 == 0) {
            printf("  %ld pedidos...\r", i + 1);
            fflush(stdout);
        }
    }
    
    printf("\n");
    
    // Converte para array e ordena
    int categoryCount = 0;
    CATEGORY* categories = malloc(1000 * sizeof(CATEGORY));
    
    for (int i = 0; i < 1000; i++) {
        CategoryNode* current = categoryHash[i];
        while (current) {
            categories[categoryCount].category_id = current->category_id;
            strncpy(categories[categoryCount].category_alias, current->category_alias, 
                    sizeof(categories[categoryCount].category_alias)-1);
            categories[categoryCount].product_count = current->product_count;
            categories[categoryCount].total_sales = current->total_sales;
            categories[categoryCount].total_revenue = current->total_revenue;
            categoryCount++;
            
            CategoryNode* temp = current;
            current = current->next;
            free(temp);
        }
    }
    
    qsort(categories, categoryCount, sizeof(CATEGORY), compareCategory);
    
    // Reescreve arquivo
    fseek(categoryRegister, 0, SEEK_SET);
    fseek(categoryIndex, 0, SEEK_SET);
    
    for (int i = 0; i < categoryCount; i++) {
        long pos = i * sizeof(CATEGORY);
        fwrite(&categories[i], sizeof(CATEGORY), 1, categoryRegister);
        
        if (i % indexGap == 0) {
            INDEX indexEntry = {categories[i].category_id, pos};
            fwrite(&indexEntry, sizeof(INDEX), 1, categoryIndex);
        }
    }
    
    ftruncate(fileno(categoryRegister), categoryCount * sizeof(CATEGORY));
    ftruncate(fileno(categoryIndex), ((categoryCount + indexGap - 1) / indexGap) * sizeof(INDEX));
    
    fflush(categoryRegister);
    fflush(categoryIndex);
    
    free(categories);
    free(categoryHash);
    
    printf("Categorias reconstruidas: %d registros\n\n", categoryCount);
}

// RESPONDE: Qual a categoria com mais itens vendidos?
void findBestSellingCategory(FILE* categoryRegister) {
    printf("\n=== CATEGORIA COM MAIS ITENS VENDIDOS ===\n\n");
    
    fseek(categoryRegister, 0, SEEK_END);
    long totalCategories = ftell(categoryRegister) / sizeof(CATEGORY);
    
    if (totalCategories == 0) {
        printf("Nenhuma categoria encontrada.\n");
        return;
    }
    
    printf("Analisando %ld categorias...\n\n", totalCategories);
    
    // Carrega todas as categorias
    CATEGORY* categories = malloc(totalCategories * sizeof(CATEGORY));
    fseek(categoryRegister, 0, SEEK_SET);
    fread(categories, sizeof(CATEGORY), totalCategories, categoryRegister);
    
    // Ordena por total_sales
    qsort(categories, totalCategories, sizeof(CATEGORY), compareCategorySales);
    
    // Exibe TOP 10
    printf("=== TOP 10 CATEGORIAS ===\n");
            printf("%-4s %-15s %-30s %-12s %-15s %-12s\n",
           "Pos", "Category ID", "Alias", "Produtos", "Vendas", "Receita");
    printf("------------------------------------------------------------------------------------\n");
    
    int limit = (totalCategories < 10) ? totalCategories : 10;
    for (int i = 0; i < limit; i++) {
        printf("%-4d %-15lld %-30s %-12d %-15d $%-11.2f\n",
               i + 1,
               categories[i].category_id,
               categories[i].category_alias,
               categories[i].product_count,
               categories[i].total_sales,
               categories[i].total_revenue);
    }
    printf("------------------------------------------------------------------------------------\n\n");
    
    // Destaque para o campeão
    printf("*** CATEGORIA MAIS VENDIDA ***\n");
    printf("Category ID:     %lld\n", categories[0].category_id);
    printf("Nome:            %s\n", categories[0].category_alias);
    printf("Produtos:        %d tipos\n", categories[0].product_count);
    printf("Total vendido:   %d unidades\n", categories[0].total_sales);
    printf("Receita total:   $%.2f\n", categories[0].total_revenue);
    printf("Ticket medio:    $%.2f\n", categories[0].total_revenue / categories[0].total_sales);
    printf("\n");
    
    free(categories);
}

// Lista todas as categorias
void listAllCategories(FILE* categoryRegister) {
    printf("\n=== TODAS AS CATEGORIAS ===\n");
    
    fseek(categoryRegister, 0, SEEK_END);
    long total = ftell(categoryRegister) / sizeof(CATEGORY);
    
    printf("Total: %ld categorias\n\n", total);
    
    printf("%-15s %-30s %-12s %-12s %-15s\n",
           "Category ID", "Alias", "Produtos", "Vendas", "Receita");
    printf("--------------------------------------------------------------------------------\n");
    
    CATEGORY category;
    fseek(categoryRegister, 0, SEEK_SET);
    
    while (fread(&category, sizeof(CATEGORY), 1, categoryRegister) == 1) {
        printf("%-15lld %-30s %-12d %-12d $%-14.2f\n",
               category.category_id,
               category.category_alias,
               category.product_count,
               category.total_sales,
               category.total_revenue);
    }
    
    printf("--------------------------------------------------------------------------------\n\n");
}

int isOrderRemoved(ORDER* order) {
    return (order->data[0] == REMOVED_FLAG);
}

ORDER* searchOrderById(FILE* orderHistory, FILE* orderIndex, FILE* orderOverflow,
                       long long int target_id, int indexGap) {
    
    fseek(orderIndex, 0, SEEK_END);
    long fileSize = ftell(orderIndex);
    int totalEntries = fileSize / sizeof(INDEX);
    
    if (totalEntries == 0) return NULL;
    
    int left = 0, right = totalEntries - 1;
    long startPosition = 0;
    INDEX currentIndex;
    
    while (left <= right) {
        int middle = left + (right - left) / 2;
        fseek(orderIndex, middle * sizeof(INDEX), SEEK_SET);
        fread(&currentIndex, sizeof(INDEX), 1, orderIndex);
        
        if (currentIndex.id == target_id) {
            startPosition = currentIndex.position;
            break;
        }
        
        if (currentIndex.id < target_id) {
            startPosition = currentIndex.position;
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }
    
    ORDER* order = malloc(sizeof(ORDER));
    if (!order) return NULL;
    
    fseek(orderHistory, startPosition, SEEK_SET);
    
    for (int i = 0; i < indexGap; i++) {
        if (fread(order, sizeof(ORDER), 1, orderHistory) != 1) break;
        
        if (order->order_id == target_id && !isOrderRemoved(order)) {
            return order;
        }
        if (order->order_id > target_id) break;
    }
    
    if (orderOverflow) {
        fseek(orderOverflow, 0, SEEK_SET);
        OVERFLOW_RECORD overflow;
        
        while (fread(&overflow, sizeof(OVERFLOW_RECORD), 1, orderOverflow) == 1) {
            if (overflow.record.order_id == target_id && !isOrderRemoved(&overflow.record)) {
                *order = overflow.record;
                return order;
            }
        }
    }
    
    free(order);
    return NULL;
}

int insertOrderWithOverflow(ORDER* newOrder, FILE* orderHistory, FILE* orderIndex, 
                            FILE* orderOverflow, FILE* categoryRegister, 
                            FILE* categoryIndex, int indexGap) {
    
    fseek(orderHistory, 0, SEEK_END);
    long fileSize = ftell(orderHistory);
    long totalRecords = fileSize / sizeof(ORDER);
    
    if (totalRecords == 0) {
        fwrite(newOrder, sizeof(ORDER), 1, orderHistory);
        fflush(orderHistory);
        
        INDEX indexEntry = {newOrder->order_id, 0};
        fwrite(&indexEntry, sizeof(INDEX), 1, orderIndex);
        fflush(orderIndex);
        
        // Atualiza categoria
        updateCategorySales(categoryRegister, categoryIndex, newOrder->category_id,
                          newOrder->quantity, newOrder->price_usd * newOrder->quantity, indexGap);
        return 1;
    }
    
    fseek(orderIndex, 0, SEEK_END);
    int totalIndices = ftell(orderIndex) / sizeof(INDEX);
    
    int left = 0, right = totalIndices - 1;
    long blockStart = 0;
    INDEX idx;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        fseek(orderIndex, mid * sizeof(INDEX), SEEK_SET);
        fread(&idx, sizeof(INDEX), 1, orderIndex);
        
        if (idx.id <= newOrder->order_id) {
            blockStart = idx.position;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    ORDER testOrder;
    fseek(orderHistory, blockStart, SEEK_SET);
    int blockRecords = 0;
    
    for (int i = 0; i < BLOCK_SIZE && blockStart + i * sizeof(ORDER) < fileSize; i++) {
        if (fread(&testOrder, sizeof(ORDER), 1, orderHistory) != 1) break;
        blockRecords++;
    }
    
    if (blockRecords >= BLOCK_SIZE) {
        printf("Bloco cheio, usando overflow...\n");
        
        OVERFLOW_RECORD overflow;
        overflow.record = *newOrder;
        overflow.originalBlockPos = blockStart;
        overflow.nextOverflow = -1;
        
        fseek(orderOverflow, 0, SEEK_END);
        fwrite(&overflow, sizeof(OVERFLOW_RECORD), 1, orderOverflow);
        fflush(orderOverflow);
        
        printf("Registro inserido em overflow\n");
    } else {
        fseek(orderHistory, 0, SEEK_END);
        fwrite(newOrder, sizeof(ORDER), 1, orderHistory);
        fflush(orderHistory);
        printf("Registro inserido no final\n");
    }
    
    // Atualiza categoria
    updateCategorySales(categoryRegister, categoryIndex, newOrder->category_id,
                       newOrder->quantity, newOrder->price_usd * newOrder->quantity, indexGap);
    
    return 1;
}

JEWELRY* searchJewelryById(FILE* jewelryRegister, FILE* jewelryIndex, 
                          long long int product_id, int indexGap) {
    
    fseek(jewelryIndex, 0, SEEK_END);
    long fileSize = ftell(jewelryIndex);
    int totalEntries = fileSize / sizeof(INDEX);
    
    if (totalEntries == 0) return NULL;
    
    int left = 0, right = totalEntries - 1;
    long startPosition = 0;
    INDEX currentIndex;
    
    while (left <= right) {
        int middle = left + (right - left) / 2;
        fseek(jewelryIndex, middle * sizeof(INDEX), SEEK_SET);
        fread(&currentIndex, sizeof(INDEX), 1, jewelryIndex);
        
        if (currentIndex.id == product_id) {
            startPosition = currentIndex.position;
            break;
        }
        
        if (currentIndex.id < product_id) {
            startPosition = currentIndex.position;
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }
    
    JEWELRY* jewelry = malloc(sizeof(JEWELRY));
    if (!jewelry) return NULL;
    
    fseek(jewelryRegister, startPosition, SEEK_SET);
    
    for (int i = 0; i < indexGap; i++) {
        if (fread(jewelry, sizeof(JEWELRY), 1, jewelryRegister) != 1) break;
        
        if (jewelry->product_id == product_id) return jewelry;
        if (jewelry->product_id > product_id) break;
    }
    
    free(jewelry);
    return NULL;
}

int compareSales(const void* a, const void* b) {
    PRODUCT_SALES* saleA = (PRODUCT_SALES*)a;
    PRODUCT_SALES* saleB = (PRODUCT_SALES*)b;
    if (saleA->total_quantity > saleB->total_quantity) return -1;
    if (saleA->total_quantity < saleB->total_quantity) return 1;
    return 0;
}

unsigned long hash(long long int product_id) {
    return product_id % HASH_SIZE;
}

void contMostSoldJewel(FILE* orderHistory, FILE* jewelryRegister, FILE* jewelryIndex, int indexGap) {
    printf("\n=== PRODUTO MAIS VENDIDO ===\n");
    
    HashNode** hashTable = calloc(HASH_SIZE, sizeof(HashNode*));
    if (!hashTable) {
        printf("Erro ao alocar hash table.\n");
        return;
    }
    
    fseek(orderHistory, 0, SEEK_END);
    long totalOrders = ftell(orderHistory) / sizeof(ORDER);
    
    printf("Processando %ld pedidos...\n", totalOrders);
    
    ORDER order;
    fseek(orderHistory, 0, SEEK_SET);
    int uniqueProducts = 0;
    
    for (long i = 0; i < totalOrders; i++) {
        if (fread(&order, sizeof(ORDER), 1, orderHistory) != 1) break;
        if (isOrderRemoved(&order)) continue;
        
        unsigned long idx = hash(order.product_id);
        HashNode* current = hashTable[idx];
        HashNode* found = NULL;
        
        while (current) {
            if (current->product_id == order.product_id) {
                found = current;
                break;
            }
            current = current->next;
        }
        
        if (found) {
            found->total_quantity += order.quantity;
        } else {
            HashNode* newNode = malloc(sizeof(HashNode));
            newNode->product_id = order.product_id;
            newNode->total_quantity = order.quantity;
            newNode->next = hashTable[idx];
            hashTable[idx] = newNode;
            uniqueProducts++;
        }
        
        if ((i + 1) % 10000 == 0) {
            printf("  %ld pedidos...\r", i + 1);
            fflush(stdout);
        }
    }
    
    printf("\nProdutos unicos: %d\n", uniqueProducts);
    
    PRODUCT_SALES* sales = malloc(uniqueProducts * sizeof(PRODUCT_SALES));
    int count = 0;
    
    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode* current = hashTable[i];
        while (current) {
            sales[count].product_id = current->product_id;
            sales[count].total_quantity = current->total_quantity;
            count++;
            HashNode* temp = current;
            current = current->next;
            free(temp);
        }
    }
    free(hashTable);
    
    qsort(sales, count, sizeof(PRODUCT_SALES), compareSales);
    
    printf("\n=== TOP 10 ===\n");
    printf("%-4s %-20s %-12s %-10s %-10s %-10s\n", 
           "Pos", "Product ID", "Quantidade", "Cor", "Metal", "Gema");
    printf("----------------------------------------------------------------\n");
    
    int limit = (count < 10) ? count : 10;
    for (int i = 0; i < limit; i++) {
        JEWELRY* j = searchJewelryById(jewelryRegister, jewelryIndex, sales[i].product_id, indexGap);
        
        if (j) {
            printf("%-4d %-20lld %-12d %-10s %-10s %-10s\n",
                   i + 1, sales[i].product_id, sales[i].total_quantity,
                   j->color, j->metal, j->gem);
            free(j);
        }
    }
    printf("----------------------------------------------------------------\n\n");
    
    free(sales);
}

int parseYearMonth(const char* date_str, int* year, int* month) {
    if (strlen(date_str) < 7) return 0;
    
    char year_str[5] = {0};
    strncpy(year_str, date_str, 4);
    *year = atoi(year_str);
    
    char month_str[3] = {0};
    month_str[0] = date_str[5];
    month_str[1] = date_str[6];
    *month = atoi(month_str);
    
    if (*year < 2000 || *year > 2100 || *month < 1 || *month > 12) return 0;
    return 1;
}

const char* getMonthName(int month) {
    static const char* months[] = {"Jan", "Fev", "Mar", "Abr", "Mai", "Jun",
                                   "Jul", "Ago", "Set", "Out", "Nov", "Dez"};
    if (month < 1 || month > 12) return "?";
    return months[month - 1];
}

void findBestMonth(FILE* orderHistory) {
    printf("\n=== MES COM MAIS VENDAS ===\n");
    
    fseek(orderHistory, 0, SEEK_END);
    long totalOrders = ftell(orderHistory) / sizeof(ORDER);
    
    int month_qty[12] = {0};
    ORDER order;
    fseek(orderHistory, 0, SEEK_SET);
    
    for (long i = 0; i < totalOrders; i++) {
        if (fread(&order, sizeof(ORDER), 1, orderHistory) != 1) break;
        if (isOrderRemoved(&order)) continue;
        
        int year, month;
        if (parseYearMonth(order.data, &year, &month)) {
            month_qty[month - 1] += order.quantity;
        }
    }
    
    int best = 0;
    for (int i = 1; i < 12; i++) {
        if (month_qty[i] > month_qty[best]) best = i;
    }
    
    printf("\nMelhor mes: %s\n", getMonthName(best + 1));
    printf("Quantidade vendida: %d unidades\n\n", month_qty[best]);
}

int rebuildIndex(FILE* orderHistory, FILE* orderIndex, int indexGap) {
    printf("\n=== RECONSTRUINDO INDICE ===\n");
    
    fseek(orderHistory, 0, SEEK_END);
    long totalRecords = ftell(orderHistory) / sizeof(ORDER);
    
    fseek(orderIndex, 0, SEEK_SET);
    
    ORDER order;
    int indexCount = 0;
    int activeCount = 0;
    
    fseek(orderHistory, 0, SEEK_SET);
    
    for (long i = 0; i < totalRecords; i++) {
        long currentPos = ftell(orderHistory);
        if (fread(&order, sizeof(ORDER), 1, orderHistory) != 1) break;
        
        if (isOrderRemoved(&order)) continue;
        
        activeCount++;
        
        if (activeCount % indexGap == 1 || activeCount == 1) {
            INDEX indexEntry = {order.order_id, currentPos};
            fwrite(&indexEntry, sizeof(INDEX), 1, orderIndex);
            indexCount++;
        }
    }
    
    ftruncate(fileno(orderIndex), indexCount * sizeof(INDEX));
    fflush(orderIndex);
    
    printf("Indice reconstruido: %d entradas\n\n", indexCount);
    return indexCount;
}

void showOrderHistory(FILE* orderHistory) {
    fseek(orderHistory, 0, SEEK_END);
    long total = ftell(orderHistory) / sizeof(ORDER);
    
    printf("\nTotal registros: %ld\n", total);
    
    ORDER order;
    fseek(orderHistory, 0, SEEK_SET);
    
    printf("\nPrimeiros 5:\n");
    for (int i = 0; i < 5 && i < total; i++) {
        if (fread(&order, sizeof(ORDER), 1, orderHistory) == 1) {
            printf("%d. ID: %lld, Data: %s\n", i+1, order.order_id, order.data);
        }
    }
    
    printf("\nUltimos 5:\n");
    long start = (total > 5) ? total - 5 : 0;
    fseek(orderHistory, start * sizeof(ORDER), SEEK_SET);
    
    for (int i = 0; i < 5 && start + i < total; i++) {
        if (fread(&order, sizeof(ORDER), 1, orderHistory) == 1) {
            printf("%ld. ID: %lld, Data: %s\n", start + i + 1, order.order_id, order.data);
        }
    }
    printf("\n");
}

void showJewelryRegister(FILE* jewelryRegister) {
    fseek(jewelryRegister, 0, SEEK_END);
    long total = ftell(jewelryRegister) / sizeof(JEWELRY);
    
    printf("\nTotal joias: %ld\n", total);
    
    JEWELRY jewelry;
    fseek(jewelryRegister, 0, SEEK_SET);
    
    printf("\nPrimeiras 5:\n");
    for (int i = 0; i < 5 && i < total; i++) {
        if (fread(&jewelry, sizeof(JEWELRY), 1, jewelryRegister) == 1) {
            printf("%d. Product ID: %lld, Cor: %s\n", i+1, jewelry.product_id, jewelry.color);
        }
    }
    printf("\n");
}

void showFileStats(FILE* orderHistory, FILE* orderOverflow) {
    printf("\n=== ESTATISTICAS ===\n");
    
    fseek(orderHistory, 0, SEEK_END);
    long totalRecords = ftell(orderHistory) / sizeof(ORDER);
    
    ORDER order;
    int active = 0, removed = 0;
    
    fseek(orderHistory, 0, SEEK_SET);
    for (long i = 0; i < totalRecords; i++) {
        if (fread(&order, sizeof(ORDER), 1, orderHistory) != 1) break;
        if (isOrderRemoved(&order)) removed++;
        else active++;
    }
    
    int overflowCount = 0;
    if (orderOverflow) {
        fseek(orderOverflow, 0, SEEK_END);
        overflowCount = ftell(orderOverflow) / sizeof(OVERFLOW_RECORD);
    }
    
    printf("Registros totais:    %ld\n", totalRecords);
    printf("Registros ativos:    %d (%.1f%%)\n", active, (active * 100.0) / totalRecords);
    printf("Registros removidos: %d (%.1f%%)\n", removed, (removed * 100.0) / totalRecords);
    printf("Registros overflow:  %d\n", overflowCount);
    printf("Tamanho arquivo:     %.2f MB\n", (totalRecords * sizeof(ORDER)) / (1024.0 * 1024.0));
    
    if (overflowCount > totalRecords * 0.05) {
        printf("\nAVISO: Muitos registros em overflow (>5%%)\n");
        printf("Recomenda-se reorganizar o arquivo!\n");
    }
    printf("\n");
}

int removeOrder(FILE* orderHistory, FILE* orderIndex, FILE* orderOverflow, 
                long long int target_id, int indexGap) {
    printf("\n=== REMOVENDO ORDEM %lld ===\n", target_id);
    
    ORDER* order = searchOrderById(orderHistory, orderIndex, orderOverflow, target_id, indexGap);
    
    if (!order) {
        printf("Ordem nao encontrada.\n");
        return 0;
    }
    
    printf("Ordem encontrada:\n");
    printf("  Order ID: %lld\n", order->order_id);
    printf("  Data: %s\n", order->data);
    printf("  Quantidade: %d\n", order->quantity);
    printf("  Preco: %.2f USD\n", order->price_usd);
    
    printf("\nConfirma remocao? (s/n): ");
    char confirm;
    scanf(" %c", &confirm);
    
    if (confirm != 's' && confirm != 'S') {
        printf("Remocao cancelada.\n");
        free(order);
        return 0;
    }
    
    order->data[0] = REMOVED_FLAG;
    
    fseek(orderHistory, 0, SEEK_SET);
    ORDER temp;
    long pos = 0;
    int found = 0;
    
    while (fread(&temp, sizeof(ORDER), 1, orderHistory) == 1) {
        if (temp.order_id == target_id) {
            found = 1;
            break;
        }
        pos += sizeof(ORDER);
    }
    
    if (found) {
        fseek(orderHistory, pos, SEEK_SET);
        fwrite(order, sizeof(ORDER), 1, orderHistory);
        fflush(orderHistory);
    } else {
        if (orderOverflow) {
            fseek(orderOverflow, 0, SEEK_SET);
            OVERFLOW_RECORD overflow;
            long overflowPos = 0;
            
            while (fread(&overflow, sizeof(OVERFLOW_RECORD), 1, orderOverflow) == 1) {
                if (overflow.record.order_id == target_id) {
                    overflow.record.data[0] = REMOVED_FLAG;
                    fseek(orderOverflow, overflowPos, SEEK_SET);
                    fwrite(&overflow, sizeof(OVERFLOW_RECORD), 1, orderOverflow);
                    fflush(orderOverflow);
                    found = 1;
                    break;
                }
                overflowPos += sizeof(OVERFLOW_RECORD);
            }
        }
    }
    
    free(order);
    
    if (found) {
        printf("Ordem removida com sucesso.\n");
        removal_count++;
        
        if (removal_count >= REBUILD_THRESHOLD) {
            printf("\nReconstruindo indice automaticamente...\n");
            rebuildIndex(orderHistory, orderIndex, indexGap);
            removal_count = 0;
        }
        return 1;
    } else {
        printf("Erro ao localizar ordem para remocao.\n");
        return 0;
    }
}

void listOverflowRecords(FILE* orderOverflow) {
    if (!orderOverflow) {
        printf("Arquivo de overflow nao disponivel.\n");
        return;
    }
    
    printf("\n=== REGISTROS EM OVERFLOW ===\n");
    
    fseek(orderOverflow, 0, SEEK_END);
    long total = ftell(orderOverflow) / sizeof(OVERFLOW_RECORD);
    
    if (total == 0) {
        printf("Nenhum registro em overflow.\n\n");
        return;
    }
    
    printf("Total: %ld registros\n\n", total);
    printf("%-20s %-15s %-12s\n", "Order ID", "Product ID", "Quantidade");
    printf("---------------------------------------------------\n");
    
    OVERFLOW_RECORD overflow;
    fseek(orderOverflow, 0, SEEK_SET);
    int count = 0;
    
    while (fread(&overflow, sizeof(OVERFLOW_RECORD), 1, orderOverflow) == 1) {
        if (!isOrderRemoved(&overflow.record)) {
            printf("%-20lld %-15lld %-12d\n",
                   overflow.record.order_id,
                   overflow.record.product_id,
                   overflow.record.quantity);
            count++;
        }
    }
    
    printf("---------------------------------------------------\n");
    printf("Total ativos: %d\n\n", count);
}

int main(){
    printf("=== SISTEMA ISAM JOIAS ===\n\n");
    
    FILE* csv = openFile("../data/jewelry.csv", "r");
    FILE* orderHistory = openFile("../data/orderHistory.dat", "wb+");
    FILE* orderIndex = openFile("../data/orderIndex.dat", "wb+");
    FILE* jewelryRegister = openFile("../data/jewelryRegister.dat", "wb+");
    FILE* jewelryIndex = openFile("../data/jewelryIndex.dat", "wb+");
    FILE* categoryRegister = openFile("../data/categoryRegister.dat", "wb+");
    FILE* categoryIndex = openFile("../data/categoryIndex.dat", "wb+");
    FILE* orderOverflow = openFile("../data/orderOverflow.dat", "wb+");

    int indexGap = 100;

    readCSVExternalSort(csv, orderHistory, orderIndex, jewelryRegister, jewelryIndex,
                       categoryRegister, categoryIndex, indexGap);

    fclose(csv);
    fclose(orderHistory);
    fclose(orderIndex);
    fclose(jewelryRegister);
    fclose(jewelryIndex);
    fclose(categoryRegister);
    fclose(categoryIndex);
    fclose(orderOverflow);

    orderHistory = openFile("../data/orderHistory.dat", "rb+");
    orderIndex = openFile("../data/orderIndex.dat", "rb+");
    jewelryRegister = openFile("../data/jewelryRegister.dat", "rb+");
    jewelryIndex = openFile("../data/jewelryIndex.dat", "rb+");
    categoryRegister = openFile("../data/categoryRegister.dat", "rb+");
    categoryIndex = openFile("../data/categoryIndex.dat", "rb+");
    orderOverflow = openFile("../data/orderOverflow.dat", "rb+");

    int opcao = -1;
    while (opcao != 0) {
        printf("\n========== MENU ==========\n");
        printf("1 - Mostrar ordens\n");
        printf("2 - Mostrar joias\n");
        printf("3 - Buscar ordem\n");
        printf("4 - Inserir ordem\n");
        printf("5 - Remover ordem\n");
        printf("6 - Produto mais vendido\n");
        printf("7 - Mes com mais vendas\n");
        printf("8 - Categoria mais vendida\n");     // NOVO
        printf("9 - Listar categorias\n");          // NOVO
        printf("10 - Reconstruir categorias\n");    // NOVO
        printf("11 - Estatisticas\n");
        printf("12 - Listar overflow\n");
        printf("13 - Reconstruir indice\n");
        printf("0 - Sair\n");
        printf("==========================\n");
        printf("Opcao: ");
        scanf("%d", &opcao);

        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);

        switch (opcao) {
            case 1:
                showOrderHistory(orderHistory);
                break;
                
            case 2:
                showJewelryRegister(jewelryRegister);
                break;
                
            case 3: {
                long long int orderSearch;
                printf("ID da ordem: ");
                scanf("%lld", &orderSearch);
                
                ORDER* result = searchOrderById(orderHistory, orderIndex, orderOverflow, 
                                               orderSearch, indexGap);
                
                if (result) {
                    printf("\n=== ORDEM ENCONTRADA ===\n");
                    printf("Order ID:    %lld\n", result->order_id);
                    printf("Data:        %s\n", result->data);
                    printf("Product ID:  %lld\n", result->product_id);
                    printf("Quantidade:  %d\n", result->quantity);
                    printf("Preco:       $%.2f\n", result->price_usd);
                    printf("User ID:     %lld\n", result->user_id);
                    free(result);
                } else {
                    printf("Ordem nao encontrada.\n");
                }
                break;
            }
            
            case 4: {
                printf("\n=== INSERIR NOVA ORDEM ===\n");
                ORDER newOrder = {0};
                
                printf("Order ID: ");
                scanf("%lld", &newOrder.order_id);
                printf("Product ID: ");
                scanf("%lld", &newOrder.product_id);
                printf("Category ID: ");
                scanf("%lld", &newOrder.category_id);
                printf("Quantidade: ");
                scanf("%d", &newOrder.quantity);
                printf("Preco USD: ");
                scanf("%f", &newOrder.price_usd);
                printf("User ID: ");
                scanf("%lld", &newOrder.user_id);
                printf("Data (YYYY-MM-DD): ");
                scanf("%s", newOrder.data);
                
                insertOrderWithOverflow(&newOrder, orderHistory, orderIndex, orderOverflow,
                                       categoryRegister, categoryIndex, indexGap);
                break;
            }
            case 5: {
                long long int orderToRemove;
                printf("ID da ordem a remover: ");
                scanf("%lld", &orderToRemove);
                
                removeOrder(orderHistory, orderIndex, orderOverflow, 
                           orderToRemove, indexGap);
                break;
            }
            
            case 6:
                contMostSoldJewel(orderHistory, jewelryRegister, jewelryIndex, indexGap);
                break;
                
            case 7:
                findBestMonth(orderHistory);
                break;
                
            case 8:  // CATEGORIA MAIS VENDIDA
                findBestSellingCategory(categoryRegister);
                break;
                
            case 9:  // LISTAR TODAS AS CATEGORIAS
                listAllCategories(categoryRegister);
                break;
                
            case 10:  // RECONSTRUIR CATEGORIAS
                rebuildCategoryData(orderHistory, categoryRegister, categoryIndex, indexGap);
                break;
                
            case 11:
                showFileStats(orderHistory, orderOverflow);
                break;
                
            case 12:
                listOverflowRecords(orderOverflow);
                break;
                
            case 13:
                rebuildIndex(orderHistory, orderIndex, indexGap);
                break;
                
            case 0:
                printf("Encerrando sistema...\n");
                break;
                
            default:
                printf("Opcao invalida!\n");
        }
    }

    // Cleanup
    if (orderHistory) fclose(orderHistory);
    if (orderIndex) fclose(orderIndex);
    if (jewelryRegister) fclose(jewelryRegister);
    if (jewelryIndex) fclose(jewelryIndex);
    if (categoryRegister) fclose(categoryRegister);
    if (categoryIndex) fclose(categoryIndex);
    if (orderOverflow) fclose(orderOverflow);

    printf("\nSistema encerrado.\n");
    return 0;
}