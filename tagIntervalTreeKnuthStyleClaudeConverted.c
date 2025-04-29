#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

// Structure for interval node
typedef struct IntervalNode {
    double interval[2];             // [a_v, b_v] where a_v < b_v
    char* tag;                      // tag from set α or NULL
    struct IntervalNode** children; // Dynamic array of children
    int children_count;             // Number of children
    int children_capacity;          // Capacity of children array
} IntervalNode;

// Structure for rehook nodes result
typedef struct {
    bool removed;
    char* state;
    double remaining_interval[2];
    IntervalNode** rehook_node_list;
    int rehook_count;
    int rehook_capacity;
} RemoveTagResult;

// Structure for insertion points
typedef struct {
    int index;
    double start;
    double end;
} InsertionPoint;

// Function prototypes
IntervalNode* create_interval_node(double a, double b, char* tag);
void free_interval_node(IntervalNode* node);
int find_insertion_point(IntervalNode** children, int count, double value);
void add_tag_dfs(IntervalNode* node, char* tag, double a, double b);
bool try_merge_with_neighbors(IntervalNode* node, double a, double b, char* tag);
RemoveTagResult remove_tag_dfs(IntervalNode* node, char* tag, double a, double b);
bool check_tag_dfs(IntervalNode* node, char* tag, double a, double b);
char* format_text_with_tags(IntervalNode* root, const char* text);
void collect_markers(IntervalNode* node, void* markers_array, char** parent_tags, int parent_tag_count);
void add_child(IntervalNode* parent, IntervalNode* child);

// T1. Initialize tree
IntervalNode* initialize_tree(double a, double b) {
    return create_interval_node(a, b, NULL);
}

// Create interval node helper
IntervalNode* create_interval_node(double a, double b, char* tag) {
    IntervalNode* node = (IntervalNode*)malloc(sizeof(IntervalNode));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    node->interval[0] = a;
    node->interval[1] = b;
    
    if (tag) {
        node->tag = strdup(tag);
        if (!node->tag) {
            fprintf(stderr, "Memory allocation failed for tag\n");
            free(node);
            exit(EXIT_FAILURE);
        }
    } else {
        node->tag = NULL;
    }
    
    node->children_count = 0;
    node->children_capacity = 10;  // Initial capacity
    node->children = (IntervalNode**)malloc(node->children_capacity * sizeof(IntervalNode*));
    
    if (!node->children) {
        fprintf(stderr, "Memory allocation failed for children\n");
        if (node->tag) free(node->tag);
        free(node);
        exit(EXIT_FAILURE);
    }
    
    return node;
}

// Free interval node and all its children
void free_interval_node(IntervalNode* node) {
    if (!node) return;
    
    // Free tag if exists
    if (node->tag) {
        free(node->tag);
    }
    
    // Free all children recursively
    for (int i = 0; i < node->children_count; i++) {
        free_interval_node(node->children[i]);
    }
    
    // Free children array
    free(node->children);
    
    // Free node itself
    free(node);
}

// Add child to node
void add_child(IntervalNode* parent, IntervalNode* child) {
    if (parent->children_count >= parent->children_capacity) {
        parent->children_capacity *= 2;
        parent->children = (IntervalNode**)realloc(parent->children, 
                                                 parent->children_capacity * sizeof(IntervalNode*));
        if (!parent->children) {
            fprintf(stderr, "Memory reallocation failed\n");
            exit(EXIT_FAILURE);
        }
    }
    
    parent->children[parent->children_count] = child;
    parent->children_count++;
}

// Insert node at specific position
void insert_node_at_position(IntervalNode* parent, IntervalNode* child, int position) {
    if (parent->children_count >= parent->children_capacity) {
        parent->children_capacity *= 2;
        parent->children = (IntervalNode**)realloc(parent->children, 
                                                 parent->children_capacity * sizeof(IntervalNode*));
        if (!parent->children) {
            fprintf(stderr, "Memory reallocation failed\n");
            exit(EXIT_FAILURE);
        }
    }
    
    // Shift elements to make space
    for (int i = parent->children_count; i > position; i--) {
        parent->children[i] = parent->children[i-1];
    }
    
    parent->children[position] = child;
    parent->children_count++;
}

// Remove node at specific position
void remove_node_at_position(IntervalNode* parent, int position) {
    if (position < 0 || position >= parent->children_count) {
        return;
    }
    
    // Free the node at the position
    free_interval_node(parent->children[position]);
    
    // Shift elements to fill the gap
    for (int i = position; i < parent->children_count - 1; i++) {
        parent->children[i] = parent->children[i+1];
    }
    
    parent->children_count--;
}

// T2. Binary search for insertion point
int find_insertion_point(IntervalNode** children, int count, double value) {
    if (count == 0) {
        return 0;
    }
    
    int l = 0, r = count - 1;
    int m;
    
    while (l <= r) {
        m = (l + r) / 2;
        
        if (children[m]->interval[0] == value) {
            return m;
        }
        
        if (children[m]->interval[0] < value) {
            l = m + 1;
        } else {
            r = m - 1;
        }
    }
    
    return l;
}

// T3. Add tag
void add_tag(IntervalNode* root, char* tag, double a, double b) {
    add_tag_dfs(root, tag, a, b);
}

// Add tag DFS helper
void add_tag_dfs(IntervalNode* node, char* tag, double a, double b) {
    // Constrain to node boundaries
    double a_prime = fmax(a, node->interval[0]);
    double b_prime = fmin(b, node->interval[1]);
    
    // Check if there's overlap
    if (a_prime >= b_prime) {
        return; // No overlap
    }
    
    // Check if node already has tag
    if (node->tag && strcmp(node->tag, tag) == 0) {
        return; // Already has tag
    }
    
    // If node has no children, create a child with the tag
    if (node->children_count == 0) {
        IntervalNode* child = create_interval_node(a_prime, b_prime, tag);
        add_child(node, child);
        return;
    }
    
    // Try merging with neighbors
    if (try_merge_with_neighbors(node, a_prime, b_prime, tag)) {
        return;
    }
    
    // Process intervals not covered by children
    InsertionPoint* insertion_points = NULL;
    int insertions_count = 0;
    int insertions_capacity = 10;
    
    insertion_points = (InsertionPoint*)malloc(insertions_capacity * sizeof(InsertionPoint));
    if (!insertion_points) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    double current = a_prime;
    
    // Find first relevant child
    int i = find_insertion_point(node->children, node->children_count, current);
    if (i > 0 && node->children[i-1]->interval[1] > current) {
        i = i - 1;
    }
    
    // Check gap before first child
    if (i < node->children_count && current < node->children[i]->interval[0]) {
        if (insertions_count >= insertions_capacity) {
            insertions_capacity *= 2;
            insertion_points = (InsertionPoint*)realloc(insertion_points, 
                                                    insertions_capacity * sizeof(InsertionPoint));
            if (!insertion_points) {
                fprintf(stderr, "Memory reallocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
        
        insertion_points[insertions_count].index = i;
        insertion_points[insertions_count].start = current;
        insertion_points[insertions_count].end = fmin(b_prime, node->children[i]->interval[0]);
        insertions_count++;
        
        current = fmin(b_prime, node->children[i]->interval[0]);
    }
    
    // Process overlapping children
    while (i < node->children_count && current < b_prime) {
        IntervalNode* child = node->children[i];
        
        if (current < child->interval[1]) {
            add_tag_dfs(child, tag, current, b_prime);
            current = child->interval[1];
        }
        
        if (i + 1 < node->children_count) {
            IntervalNode* next = node->children[i + 1];
            if (current < b_prime && current < next->interval[0]) {
                if (insertions_count >= insertions_capacity) {
                    insertions_capacity *= 2;
                    insertion_points = (InsertionPoint*)realloc(insertion_points, 
                                                            insertions_capacity * sizeof(InsertionPoint));
                    if (!insertion_points) {
                        fprintf(stderr, "Memory reallocation failed\n");
                        exit(EXIT_FAILURE);
                    }
                }
                
                insertion_points[insertions_count].index = i + 1;
                insertion_points[insertions_count].start = current;
                insertion_points[insertions_count].end = fmin(b_prime, next->interval[0]);
                insertions_count++;
                
                current = fmin(b_prime, next->interval[0]);
            }
        }
        
        i++;
    }
    
    // Handle remaining interval after all children
    if (current < b_prime) {
        if (insertions_count >= insertions_capacity) {
            insertions_capacity *= 2;
            insertion_points = (InsertionPoint*)realloc(insertion_points, 
                                                    insertions_capacity * sizeof(InsertionPoint));
            if (!insertion_points) {
                fprintf(stderr, "Memory reallocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
        
        insertion_points[insertions_count].index = node->children_count;
        insertion_points[insertions_count].start = current;
        insertion_points[insertions_count].end = b_prime;
        insertions_count++;
    }
    
    // Insert new nodes (in reverse to preserve indices)
    for (int j = insertions_count - 1; j >= 0; j--) {
        InsertionPoint point = insertion_points[j];
        if (!try_merge_with_neighbors(node, point.start, point.end, tag)) {
            IntervalNode* new_node = create_interval_node(point.start, point.end, tag);
            insert_node_at_position(node, new_node, point.index);
        }
    }
    
    free(insertion_points);
}

// T4. Try merge with neighbors
bool try_merge_with_neighbors(IntervalNode* node, double a, double b, char* tag) {
    if (node->children_count == 0) {
        return false;
    }
    
    // Find potential neighbors
    int i = find_insertion_point(node->children, node->children_count, a);
    
    // Try left neighbor
    if (i > 0) {
        IntervalNode* left = node->children[i - 1];
        if (left->tag && strcmp(left->tag, tag) == 0 && left->interval[1] >= a) {
            // Merge with left
            left->interval[1] = fmax(left->interval[1], b);
            
            // Check right neighbor for possible merge
            if (i < node->children_count) {
                IntervalNode* right = node->children[i];
                if (right->tag && strcmp(right->tag, tag) == 0 && left->interval[1] >= right->interval[0]) {
                    // Merge left and right
                    left->interval[1] = fmax(left->interval[1], right->interval[1]);
                    
                    // Append right children to left
                    for (int j = 0; j < right->children_count; j++) {
                        add_child(left, right->children[j]);
                    }
                    
                    // Set to NULL to prevent freeing the children
                    right->children_count = 0;
                    
                    // Remove the right node
                    remove_node_at_position(node, i);
                }
            }
            
            return true;
        }
    }
    
    // Try right neighbor
    if (i < node->children_count) {
        IntervalNode* right = node->children[i];
        if (right->tag && strcmp(right->tag, tag) == 0 && b >= right->interval[0]) {
            right->interval[0] = fmin(right->interval[0], a);
            return true;
        }
    }
    
    return false;
}

// Initialize RemoveTagResult
RemoveTagResult create_remove_tag_result(bool removed, const char* state, double a, double b) {
    RemoveTagResult result;
    result.removed = removed;
    result.state = strdup(state);
    result.remaining_interval[0] = a;
    result.remaining_interval[1] = b;
    result.rehook_count = 0;
    result.rehook_capacity = 10;
    result.rehook_node_list = (IntervalNode**)malloc(result.rehook_capacity * sizeof(IntervalNode*));
    
    if (!result.rehook_node_list) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    return result;
}

// Free RemoveTagResult
void free_remove_tag_result(RemoveTagResult* result) {
    if (result->state) {
        free(result->state);
    }
    
    // Do not free the nodes themselves, they're just references
    if (result->rehook_node_list) {
        free(result->rehook_node_list);
    }
}

// Add node to rehook list
void add_to_rehook_list(RemoveTagResult* result, IntervalNode* node) {
    if (result->rehook_count >= result->rehook_capacity) {
        result->rehook_capacity *= 2;
        result->rehook_node_list = (IntervalNode**)realloc(result->rehook_node_list, 
                                                        result->rehook_capacity * sizeof(IntervalNode*));
        if (!result->rehook_node_list) {
            fprintf(stderr, "Memory reallocation failed\n");
            exit(EXIT_FAILURE);
        }
    }
    
    result->rehook_node_list[result->rehook_count] = node;
    result->rehook_count++;
}

// Add rehook list from another result
void add_rehook_list(RemoveTagResult* dest, RemoveTagResult* src) {
    for (int i = 0; i < src->rehook_count; i++) {
        add_to_rehook_list(dest, src->rehook_node_list[i]);
    }
}

// T5. Remove tag
bool remove_tag(IntervalNode* root, char* tag, double a, double b) {
    RemoveTagResult result = remove_tag_dfs(root, tag, a, b);
    bool removed = result.removed;
    free_remove_tag_result(&result);
    return removed;
}

// Remove tag DFS helper
RemoveTagResult remove_tag_dfs(IntervalNode* node, char* tag, double a, double b) {
    // Adjust interval to node boundaries
    double a_prime = fmax(a, node->interval[0]);
    double b_prime = fmin(b, node->interval[1]);
    
    // Check if there's overlap
    if (a_prime >= b_prime) {
        return create_remove_tag_result(false, "NO_OVERLAP", a, b);
    }
    
    // If node has the tag to remove
    if (node->tag && strcmp(node->tag, tag) == 0) {
        double a_v = node->interval[0];
        double b_v = node->interval[1];
        RemoveTagResult result = create_remove_tag_result(true, "", b, b); // Will set state later
        
        // Case 1: Remove interval is inside node
        if (a_prime > a_v && b_prime < b_v) {
            IntervalNode** before = (IntervalNode**)malloc(node->children_count * sizeof(IntervalNode*));
            IntervalNode** inside = (IntervalNode**)malloc(node->children_count * sizeof(IntervalNode*));
            IntervalNode** after = (IntervalNode**)malloc(node->children_count * sizeof(IntervalNode*));
            int before_count = 0, inside_count = 0, after_count = 0;
            
            // Categorize children
            for (int i = 0; i < node->children_count; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[1] <= a_prime) {
                    before[before_count++] = child;
                } else if (child->interval[0] >= b_prime) {
                    after[after_count++] = child;
                } else {
                    // Child overlaps with removal
                    RemoveTagResult child_result = remove_tag_dfs(child, tag, a_prime, b_prime);
                    
                    if (strcmp(child_result.state, "REMOVE-ENTIRE-NODE") == 0 || 
                        strcmp(child_result.state, "REMOVE-INTERVAL-INSIDE") == 0) {
                        // Add rehook nodes
                        add_rehook_list(&result, &child_result);
                    } else {
                        inside[inside_count++] = child;
                    }
                    
                    free_remove_tag_result(&child_result);
                }
            }
            
            // Create node for interval before removal
            if (a_prime > a_v) {
                IntervalNode* pre_node = create_interval_node(a_v, a_prime, tag);
                
                // Add 'before' children to pre_node
                for (int i = 0; i < before_count; i++) {
                    add_child(pre_node, before[i]);
                }
                
                add_to_rehook_list(&result, pre_node);
            } else {
                // Add 'before' children directly to rehook list
                for (int i = 0; i < before_count; i++) {
                    add_to_rehook_list(&result, before[i]);
                }
            }
            
            // Add overlapping children
            for (int i = 0; i < inside_count; i++) {
                add_to_rehook_list(&result, inside[i]);
            }
            
            // Create node for interval after removal
            if (b_prime < b_v) {
                IntervalNode* post_node = create_interval_node(b_prime, b_v, tag);
                
                // Add 'after' children to post_node
                for (int i = 0; i < after_count; i++) {
                    add_child(post_node, after[i]);
                }
                
                add_to_rehook_list(&result, post_node);
            } else {
                // Add 'after' children directly to rehook list
                for (int i = 0; i < after_count; i++) {
                    add_to_rehook_list(&result, after[i]);
                }
            }
            
            // Set state and cleanup
            free(before);
            free(inside);
            free(after);
            
            result.state = strdup("REMOVE-INTERVAL-INSIDE");
            return result;
        }
        
        // Case 2: Removal starts before/at node start, ends within node
        else if (a_prime <= a_v && b_prime < b_v) {
            node->interval[0] = b_prime;
            
            // Process affected children
            int* to_remove = (int*)malloc(node->children_count * sizeof(int));
            int remove_count = 0;
            
            for (int i = 0; i < node->children_count; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[1] <= b_prime) {
                    to_remove[remove_count++] = i;
                } else if (child->interval[0] < b_prime) {
                    RemoveTagResult child_result = remove_tag_dfs(child, tag, a_prime, b_prime);
                    
                    if (child_result.removed && strcmp(child_result.state, "REMOVE-ENTIRE-NODE") == 0) {
                        to_remove[remove_count++] = i;
                        
                        // Add rehook nodes that are after the new boundary
                        for (int j = 0; j < child_result.rehook_count; j++) {
                            IntervalNode* rehook_node = child_result.rehook_node_list[j];
                            
                            if (rehook_node->interval[0] >= b_prime) {
                                int insert_pos = find_insertion_point(node->children, node->children_count, 
                                                                    rehook_node->interval[0]);
                                insert_node_at_position(node, rehook_node, insert_pos);
                                
                                // Adjust index for removed nodes
                                for (int k = 0; k < remove_count; k++) {
                                    if (to_remove[k] >= insert_pos) {
                                        to_remove[k]++;
                                    }
                                }
                            }
                        }
                    }
                    
                    free_remove_tag_result(&child_result);
                }
            }
            
            // Remove affected children (in reverse)
            for (int i = remove_count - 1; i >= 0; i--) {
                remove_node_at_position(node, to_remove[i]);
            }
            
            free(to_remove);
            
            result.state = strdup("REMOVE-INTERVAL-LEFT");
            result.remaining_interval[0] = b_prime;
            return result;
        }
        
        // Case 3: Removal starts within node, extends to/beyond node end
        else if (a_prime > a_v && b_prime >= b_v) {
            node->interval[1] = a_prime;
            
            // Process affected children
            int* to_remove = (int*)malloc(node->children_count * sizeof(int));
            int remove_count = 0;
            
            for (int i = 0; i < node->children_count; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[0] >= a_prime) {
                    to_remove[remove_count++] = i;
                } else if (child->interval[1] > a_prime) {
                    RemoveTagResult child_result = remove_tag_dfs(child, tag, a_prime, child->interval[1]);
                    
                    if (child_result.removed && strcmp(child_result.state, "REMOVE-ENTIRE-NODE") == 0) {
                        to_remove[remove_count++] = i;
                        
                        // Add rehook nodes that are before the new boundary
                        for (int j = 0; j < child_result.rehook_count; j++) {
                            IntervalNode* rehook_node = child_result.rehook_node_list[j];
                            
                            if (rehook_node->interval[1] <= a_prime) {
                                int insert_pos = find_insertion_point(node->children, node->children_count, 
                                                                    rehook_node->interval[0]);
                                insert_node_at_position(node, rehook_node, insert_pos);
                                
                                // Adjust index for removed nodes
                                for (int k = 0; k < remove_count; k++) {
                                    if (to_remove[k] >= insert_pos) {
                                        to_remove[k]++;
                                    }
                                }
                            }
                        }
                    }
                    
                    free_remove_tag_result(&child_result);
                }
            }
            
            // Remove affected children (in reverse)
            for (int i = remove_count - 1; i >= 0; i--) {
                remove_node_at_position(node, to_remove[i]);
            }
            
            free(to_remove);
            
            result.state = strdup("REMOVE-INTERVAL-RIGHT");
            result.remaining_interval[0] = a;
            result.remaining_interval[1] = a_prime;
            return result;
        }
        
        // Case 4: Removal completely covers node
        else if (a_prime <= a_v && b_prime >= b_v) {
            // Process all children
            for (int i = 0; i < node->children_count; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[0] < b_prime && child->interval[1] > a_prime) {
                    RemoveTagResult child_result = remove_tag_dfs(child, tag, a_prime, b_prime);
                    
                    if (child_result.removed) {
                        add_rehook_list(&result, &child_result);
                    } else {
                        add_to_rehook_list(&result, child);
                    }
                    
                    free_remove_tag_result(&child_result);
                } else {
                    add_to_rehook_list(&result, child);
                }
            }
            
            // Set children to NULL to prevent them from being freed when node is removed
            node->children_count = 0;
            
            result.state = strdup("REMOVE-ENTIRE-NODE");
            result.remaining_interval[0] = b_prime;
            return result;
        }
    }
    
    // Node doesn't have tag to remove, process children
    bool removed = false;
    RemoveTagResult result = create_remove_tag_result(false, "PROCESSED_CHILDREN", fmax(b_prime, a), b);
    
    // Find children that might overlap
    int start_idx = find_insertion_point(node->children, node->children_count, a_prime);
    if (start_idx > 0 && node->children[start_idx - 1]->interval[1] > a_prime) {
        start_idx = start_idx - 1;
    }
    
    int i = start_idx;
    while (i < node->children_count) {
        IntervalNode* child = node->children[i];
        
        // Skip if no overlap
        if (b_prime <= child->interval[0] || a_prime >= child->interval[1]) {
            i++;
            continue;
        }
        
        RemoveTagResult child_result = remove_tag_dfs(child, tag, a, b);
        
        if (child_result.removed) {
            removed = true;
            result.removed = true;
            
            if (strcmp(child_result.state, "REMOVE-ENTIRE-NODE") == 0 || 
                strcmp(child_result.state, "REMOVE-INTERVAL-INSIDE") == 0) {
                // Remove child
                for (int j = i; j < node->children_count - 1; j++) {
                    node->children[j] = node->children[j + 1];
                }
                node->children_count--;
                
                // Insert rehook nodes
                if (child_result.rehook_count > 0) {
                    for (int j = 0; j < child_result.rehook_count; j++) {
                        IntervalNode* rehook_node = child_result.rehook_node_list[j];
                        int insert_pos = find_insertion_point(node->children, node->children_count, 
                                                          rehook_node->interval[0]);
                        insert_node_at_position(node, rehook_node, insert_pos);
                    }
                    
                    // Reset position
                    i = find_insertion_point(node->children, node->children_count, a_prime);
                    if (i > 0 && node->children[i - 1]->interval[1] > a_prime) {
                        i = i - 1;
                    }
                }
            } else {
                // Child was adjusted, not removed
                i++;
            }
            
            // Process remaining interval if any
            if (child_result.remaining_interval[0] < child_result.remaining_interval[1]) {
                RemoveTagResult remaining_result = remove_tag_dfs(node, tag, 
                                                                child_result.remaining_interval[0], 
                                                                child_result.remaining_interval[1]);
                if (remaining_result.removed) {
                    removed = true;
                    result.removed = true;
                }
                free_remove_tag_result(&remaining_result);
            }
        } else {
            i++;
        }
        
        free_remove_tag_result(&child_result);
    }
    
    // Ensure child intervals are properly nested
    for (int i = 0; i < node->children_count; i++) {
        IntervalNode* child = node->children[i];
        child->interval[0] = fmax(child->interval[0], node->interval[0]);
        child->interval[1] = fmin(child->interval[1], node->interval[1]);
    }
    
    return result;
}

// T6. Check for tag
bool check_tag(IntervalNode* root, char* tag, double a, double b) {
    return check_tag_dfs(root, tag, a, b);
}

// Check tag DFS helper
bool check_tag_dfs(IntervalNode* node, char* tag, double a, double b) {
    // If node has the tag and covers the entire interval
    if (node->tag && strcmp(node->tag, tag) == 0 && 
        node->interval[0] <= a && node->interval[1] >= b) {
        return true;
    }
    
    // Find children that might overlap
    int i = find_insertion_point(node->children, node->children_count, a);
    if (i > 0 && node->children[i - 1]->interval[1] > a) {
        i = i - 1;
    }
    
    // Check relevant children
    while (i < node->children_count) {
        IntervalNode* child = node->children[i];
        
        // Skip if no overlap
        if (b <= child->interval[0] || a >= child->interval[1]) {
            i++;
            continue;
        }
        
        if (check_tag_dfs(child, tag, a, b)) {
            return true;
        }
        
        i++;
    }
    
    return false;
}

// Structure for tag markers in formatted text
typedef struct {
    double position;
    char* tag;
    bool is_opening;
} TagMarker;

// Compare function for tag markers
int compare_markers(const void* a, const void* b) {
    TagMarker* marker_a = (TagMarker*)a;
    TagMarker* marker_b = (TagMarker*)b;
    
    if (marker_a->position != marker_b->position) {
        return (marker_a->position < marker_b->position) ? -1 : 1;
    }
    
    // If positions are equal, closing tags come before opening tags
    if (marker_a->is_opening != marker_b->is_opening) {
        return marker_a->is_opening ? 1 : -1;
    }
    
    return 0;
}

// T7. Format text with tags
char* format_text_with_tags(IntervalNode* root, const char* text) {
    int text_len = strlen(text);
    int capacity = 1000; // Initial capacity for markers
    
    // Allocate array for markers
    TagMarker* markers = (TagMarker*)malloc(capacity * sizeof(TagMarker));
    if (!markers) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    int markers_count = 0;
    char** parent_tags = NULL;
    int parent_count = 0;
    
    // Collect markers from tree
    collect_markers(root, markers, parent_tags, parent_count, &markers_count, &capacity);
    
    // Sort markers by position
    qsort(markers, markers_count, sizeof(TagMarker), compare_markers);
    
    // Calculate result size
    int result_size = text_len + 1; // Start with text length + null terminator
    for (int i = 0; i < markers_count; i++) {
        // Add space for tag brackets, name, and slash if closing tag
        result_size += strlen(markers[i].tag) + 2 + (markers[i].is_opening ? 0 : 1);
    }
    
    // Allocate result string
    char* result = (char*)malloc(result_size * sizeof(char));
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        free(markers);
        exit(EXIT_FAILURE);
    }
    
    // Build result with tags inserted
    int pos = 0;
    int result_pos = 0;
    
    for (int i = 0; i < markers_count; i++) {
        TagMarker marker = markers[i];
        
        // Add text segment
        int segment_len = (int)(marker.position) - pos;
        if (segment_len > 0) {
            strncpy(result + result_pos, text + pos, segment_len);
            result_pos += segment_len;
        }
        pos = (int)(marker.position);
        
        // Add tag
        if (marker.is_opening) {
            result[result_pos++] = '<';
            strcpy(result + result_pos, marker.tag);
            result_pos += strlen(marker.tag);
            result[result_pos++] = '>';
        } else {
            result[result_pos++] = '<';
            result[result_pos++] = '/';
            strcpy(result + result_pos, marker.tag);
            result_pos += strlen(marker.tag);
            result[result_pos++] = '>';
        }
    }
    
    // Add remaining text
    strcpy(result + result_pos, text + pos);
    
    // Clean up markers
    for (int i = 0; i < markers_count; i++) {
        free(markers[i].tag);
    }
    free(markers);
    
    return result;
}

// Helper for collecting markers
void collect_markers(IntervalNode* node, TagMarker* markers, char** parent_tags, int parent_count,
                   int* markers_count, int* capacity) {
    // Add marker for current node if it has a tag
    if (node->tag) {
        // Check if we need to resize markers array
        if (*markers_count + 2 > *capacity) {
            *capacity *= 2;
            markers = (TagMarker*)realloc(markers, *capacity * sizeof(TagMarker));
            if (!markers) {
                fprintf(stderr, "Memory reallocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
        
        // Add opening marker
        markers[*markers_count].position = node->interval[0];
        markers[*markers_count].tag = strdup(node->tag);
        markers[*markers_count].is_opening = true;
        (*markers_count)++;
        
        // Add closing marker
        markers[*markers_count].position = node->interval[1];
        markers[*markers_count].tag = strdup(node->tag);
        markers[*markers_count].is_opening = false;
        (*markers_count)++;
    }
    
    // Update parent tags for recursion
    int new_parent_count = parent_count;
    char** new_parent_tags = NULL;
    
    if (node->tag) {
        new_parent_count = parent_count + 1;
        new_parent_tags = (char**)malloc(new_parent_count * sizeof(char*));
        
        // Copy existing parent tags
        for (int i = 0; i < parent_count; i++) {
            new_parent_tags[i] = parent_tags[i];
        }
        
        // Add current tag
        new_parent_tags[parent_count] = node->tag;
    } else {
        new_parent_tags = parent_tags;
    }
    
    // Process children
    for (int i = 0; i < node->children_count; i++) {
        collect_markers(node->children[i], markers, new_parent_tags, new_parent_count, 
                      markers_count, capacity);
    }
    
    // Clean up
    if (node->tag && new_parent_tags != parent_tags) {
        free(new_parent_tags);
    }
}

// Main function to demonstrate usage
int main() {
    // Initialize tree with range [0, 100]
    IntervalNode* root = initialize_tree(0, 100);
    
    // Add some tags
    add_tag(root, "bold", 10, 20);
    add_tag(root, "italic", 15, 25);
    add_tag(root, "underline", 30, 40);
    
    // Check tags
    printf("Tag 'bold' in [12, 18]: %s\n", check_tag(root, "bold", 12, 18) ? "true" : "false");
    printf("Tag 'italic' in [5, 10]: %s\n", check_tag(root, "italic", 5, 10) ? "true" : "false");
    
    // Remove a tag
    remove_tag(root, "bold", 12, 18);
    printf("After removal - Tag 'bold' in [12, 18]: %s\n", 
           check_tag(root, "bold", 12, 18) ? "true" : "false");
    printf("After removal - Tag 'bold' in [10, 12]: %s\n", 
           check_tag(root, "bold", 10, 12) ? "true" : "false");
    
    // Format text with tags
    const char* text = "This is a sample text for interval tag demonstration.";
    char* formatted = format_text_with_tags(root, text);
    printf("Formatted text: %s\n", formatted);
    
    // Clean up
    free(formatted);
    free_interval_node(root);
    
    return 0;
}
