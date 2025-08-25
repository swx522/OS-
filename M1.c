#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <linux/limits.h>
#include <stdbool.h>

#define MAX_PROCESSES 1024

typedef struct Process {
    int pid;
    int ppid;
    char name[256];
    int num_children;
    struct Process** children;
} Process;

Process* processes[MAX_PROCESSES];
int process_count = 0;

void scan_proc(void);
Process* find_process(int pid);
void build_tree(void);
void print_tree(Process* root, int depth);
void free_tree(Process* root);

int main() {
    scan_proc();
    build_tree();
    
    Process* root = NULL;
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->ppid == 0) {
            root = processes[i];
            break;
        }
    }
    if (root == NULL) {
        root = find_process(1);
    }
    
    if (root != NULL) {
        print_tree(root, 0);
    } else {
        printf("No root process found!\n");
    }
    
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->children != NULL) {
            free(processes[i]->children);
        }
        free(processes[i]);
    }
    
    return 0;
}

void scan_proc() {
    DIR* dir;
    struct dirent* entry;
    
    dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir(/proc) failed");
        exit(EXIT_FAILURE);
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) != 0) {
            int pid = atoi(entry->d_name);
            
            char status_path[PATH_MAX];
            snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
            
            FILE* status_file = fopen(status_path, "r");
            if (status_file == NULL) {
                continue;
            }
            
            Process* p = malloc(sizeof(Process));
            if (p == NULL) {
                perror("malloc failed");
                fclose(status_file);
                closedir(dir);
                exit(EXIT_FAILURE);
            }
            p->pid = pid;
            p->ppid = -1;
            p->name[0] = '\0';
            p->num_children = 0;
            p->children = NULL;
            
            char line[256];
            while (fgets(line, sizeof(line), status_file)) {
                if (strncmp(line, "Name:", 5) == 0) {
                    sscanf(line, "Name:\t%s", p->name);
                } else if (strncmp(line, "PPid:", 5) == 0) {
                    sscanf(line, "PPid:\t%d", &p->ppid);
                }
                if (p->name[0] != '\0' && p->ppid != -1) {
                    break;
                }
            }
            fclose(status_file);
            
            if (p->name[0] != '\0' && p->ppid != -1) {
                processes[process_count++] = p;
                if (process_count >= MAX_PROCESSES) {
                    fprintf(stderr, "Warning: Reached maximum process limit (%d)\n", MAX_PROCESSES);
                    break;
                }
            } else {
                free(p);
            }
        }
    }
    closedir(dir);
}

Process* find_process(int pid) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == pid) {
            return processes[i];
        }
    }
    return NULL;
}

void build_tree() {
    int* child_counts = calloc(process_count, sizeof(int));
    if (child_counts == NULL) {
        perror("calloc failed");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < process_count; i++) {
        Process* child = processes[i];
        Process* parent = find_process(child->ppid);
        if (parent != NULL) {
            for (int j = 0; j < process_count; j++) {
                if (processes[j] == parent) {
                    child_counts[j]++;
                    break;
                }
            }
        }
    }
    
    for (int i = 0; i < process_count; i++) {
        if (child_counts[i] > 0) {
            processes[i]->children = malloc(child_counts[i] * sizeof(Process*));
            if (processes[i]->children == NULL) {
                perror("malloc failed");
                free(child_counts);
                exit(EXIT_FAILURE);
            }
        }
        processes[i]->num_children = 0;
    }
    
    free(child_counts);
    
    for (int i = 0; i < process_count; i++) {
        Process* child = processes[i];
        Process* parent = find_process(child->ppid);
        if (parent != NULL) {
            parent->children[parent->num_children++] = child;
        }
    }
}

void print_tree(Process* root, int depth) {
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }
    
    if (depth == 0) {
        printf("%s(%d)\n", root->name, root->pid);
    } else {
        printf("└── %s(%d)\n", root->name, root->pid);
    }
    
    for (int i = 0; i < root->num_children; i++) {
        print_tree(root->children[i], depth + 1);
    }
}

void free_tree(Process* root) {
    for (int i = 0; i < root->num_children; i++) {
        free_tree(root->children[i]);
    }
    free(root->children);
    free(root);
}
