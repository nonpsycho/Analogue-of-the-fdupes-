#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <magic.h>
#include <openssl/evp.h> 
#include <ctype.h>

#define MAX_FILES 10000
#define MAX_PATH 4096
#define BUFFER_SIZE 8192
#define HASH_SIZE 32 

static int dry_run_mode = 0;

typedef struct {
    char path[MAX_PATH];
    off_t size;
    char mime_type[128];
    unsigned char hash[HASH_SIZE];
    int is_duplicate;
    int to_delete;
} FileInfo;

FileInfo files[MAX_FILES];
int file_count = 0;


// Функциия для вычисления хеша с использованием EVP
int calculate_file_hash(const char *path, unsigned char *output) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 0;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    unsigned int md_len;

    EVP_DigestInit_ex(mdctx, md, NULL);
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file))) {
        EVP_DigestUpdate(mdctx, buffer, bytes_read);
    }
    
    EVP_DigestFinal_ex(mdctx, output, &md_len);
    EVP_MD_CTX_free(mdctx);
    fclose(file);
    
    return 1;
}

// Функция для определения MIME-типа
const char *get_mime_type(const char *path) {
    static magic_t magic_cookie = NULL;
    
    if (magic_cookie == NULL) {
        magic_cookie = magic_open(MAGIC_MIME_TYPE);
        if (magic_cookie == NULL || magic_load(magic_cookie, NULL) != 0) {
            if (magic_cookie) {
                fprintf(stderr, "magic_load failed: %s\n", magic_error(magic_cookie));
                magic_close(magic_cookie);
            }
            return "application/octet-stream";
        }
    }
    
    const char *mime = magic_file(magic_cookie, path);
    return mime ? mime : "application/octet-stream";
}

// Функция сравнения хешей
int compare_hashes(const unsigned char *hash1, const unsigned char *hash2) {
    return memcmp(hash1, hash2, HASH_SIZE) == 0;
}

// Сканирование директории с вычислением хешей и отладочной информацией
void scan_directory(const char *dir_path, const char *mime_filter) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[MAX_PATH];
    
    if ((dir = opendir(dir_path)) == NULL) {
        perror(dir_path);
        return;
    }
    
    printf("Starting scan in: %s\n", dir_path);
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (lstat(full_path, &statbuf) == -1) {
            perror(full_path);
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            scan_directory(full_path, mime_filter);
        } else if (S_ISREG(statbuf.st_mode)) {
            if (file_count >= MAX_FILES) {
                fprintf(stderr, "Maximum file limit reached\n");
                break;
            }
            
            const char *mime_type = get_mime_type(full_path);
            printf("Processing: %s (%s)\n", full_path, mime_type);
            
            if (mime_filter == NULL || strstr(mime_type, mime_filter) != NULL) {
                FileInfo *file = &files[file_count];
                strncpy(file->path, full_path, MAX_PATH);
                file->size = statbuf.st_size;
                strncpy(file->mime_type, mime_type, sizeof(file->mime_type) - 1);
                
                if (!calculate_file_hash(full_path, file->hash)) {
                    continue;
                }
                
                printf("  Hash: ");
                for (int k = 0; k < 4; k++) printf("%02x", file->hash[k]);
                printf("...\n");
                
                file->is_duplicate = 0;
                file->to_delete = 0;
                file_count++;
            }
        }
    }
    
    closedir(dir);
    printf("Scan completed. Total files: %d\n", file_count);
}

// Поиск дубликатов по хешу и размеру
void find_duplicates() {
    for (int i = 0; i < file_count; i++) {
        if (files[i].is_duplicate) continue;
        
        for (int j = i + 1; j < file_count; j++) {
            if (files[j].is_duplicate) continue;
            if (files[i].size != files[j].size) continue;
            if (!compare_hashes(files[i].hash, files[j].hash)) continue;
            
            files[j].is_duplicate = 1;
        }
    }
}

// Удаление дубликатов
void delete_duplicates(int interactive) {
    int deleted_count = 0;
    
    for (int i = 0; i < file_count; i++) {
        if (files[i].is_duplicate || files[i].to_delete) {
            if (interactive) {
                int c;
                int valid_input = 0;
                
                do {
                    printf("Delete %s? [y/N] ", files[i].path);
                    fflush(stdout);
                    
                    c = tolower(getchar());
                    while (getchar() != '\n' && !feof(stdin));
                    
                    if (c == 'y' || c == 'n' || c == '\n') {
                        valid_input = 1;
                    } else {
                        printf("Invalid input. Please enter 'y' or 'n'.\n");
                    }
                } while (!valid_input);
                
                if (c != 'y') {
                    continue; 
                }
            }
            
            printf("Deleting: %s\n", files[i].path);
            if (!dry_run_mode) {
                if (unlink(files[i].path) == 0) {
                    deleted_count++;
                } else {
                    perror("Error deleting file");
                }
            }
        }
    }
    
    printf("\nTotal deleted: %d files\n", deleted_count);
}

/*void mark_duplicates_for_deletion() {
    for (int i = 0; i < file_count; i++) {
        if (files[i].is_duplicate) continue; 
        
        for (int j = i + 1; j < file_count; j++) {
            if (files[j].is_duplicate) continue;
            if (files[i].size != files[j].size) continue;
            if (!compare_hashes(files[i].hash, files[j].hash)) continue;
            
            files[j].is_duplicate = 1;
            files[j].to_delete = 1;
        }
    }
}*/

// Вывод результатов с группировкой по MIME-типу и хешу
void print_duplicates() {
    printf("\n=== Duplicate Files Report ===\n");
    int total_duplicates = 0;
    
    for (int i = 0; i < file_count; i++) {
        if (files[i].is_duplicate) continue;
        
        int duplicate_count = 0;
        
        for (int j = i + 1; j < file_count; j++) {
            if (!files[j].is_duplicate) continue;
            if (files[i].size != files[j].size) continue;
            if (!compare_hashes(files[i].hash, files[j].hash)) continue;
            duplicate_count++;
        }
        
        if (duplicate_count == 0) continue;
        
        total_duplicates += duplicate_count;
        
        printf("\nGroup %d:\n", i+1);
        printf("MIME: %s\n", files[i].mime_type);
        printf("Size: %ld bytes\n", (long)files[i].size);
        printf("Hash: ");
        for (int k = 0; k < 8; k++) printf("%02x", files[i].hash[k]);
        printf("...\n");
        
        printf("  KEEP: %s\n", files[i].path);
        
        for (int j = i + 1; j < file_count; j++) {
            if (!files[j].is_duplicate) continue;
            if (files[i].size != files[j].size) continue;
            if (!compare_hashes(files[i].hash, files[j].hash)) continue;

            printf("  DEL:  %s\n", files[j].path);
            files[j].is_duplicate = 1;  
            files[j].to_delete = 1; 
        }
    }
    
    if (total_duplicates == 0) {
        printf("\nNo duplicates found.\n");
    } else {
        printf("\nTotal duplicates found: %d\n", total_duplicates);
    }
}

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s <directory> [mime-type] [options]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d     Delete duplicates automatically\n");
    fprintf(stderr, "  -i     Interactive delete (ask before each deletion)\n");
    fprintf(stderr, "  -n     Dry run (only show what would be deleted)\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s ~/photos image/\n", program_name);
    fprintf(stderr, "  %s ~/downloads application/zip -d\n", program_name);
    fprintf(stderr, "  %s ~/documents text/ -i\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *dir_path = argv[1];
    const char *mime_filter = NULL;
    int delete_mode = 0; // 0 = don't delete, 1 = auto delete, 2 = interactive delete
    
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'd':
                    delete_mode = 1;
                    break;
                case 'i':
                    delete_mode = 2;
                    break;
                case 'n':
                    dry_run_mode = 1;
                    break;
                default:
                    fprintf(stderr, "Unknown option: %s\n", argv[i]);
                    print_usage(argv[0]);
                    return 1;
            }
        } else if (mime_filter == NULL) {
            mime_filter = argv[i];
        }
    }
    
    printf("Scanning directory: %s", dir_path);
    if (mime_filter) printf(" (MIME filter: %s)", mime_filter);
    printf("\n");
    
    scan_directory(dir_path, mime_filter);
    printf("Found %d files\n", file_count);
    if (file_count == 0) {
        printf("No files found matching criteria.\n");
        return 0;
    }
    printf("Finding duplicates...\n");
    find_duplicates();
    print_duplicates();
    
    if (dry_run_mode) {
        printf("\n=== Duplicate Deletion ===\n");
        printf("Dry run mode - no files will be actually deleted\n");
    } else if (delete_mode == 1) {
        printf("Auto-deleting duplicates...\n");
        delete_duplicates(0);
    } else if (delete_mode == 2) {
        printf("Interactive deletion mode\n");
        delete_duplicates(1);
    }
    
    return 0;
}   