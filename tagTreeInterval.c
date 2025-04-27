
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define MAX_TAG_LENGTH 32

// Result state enum for removal operations
typedef enum {
    NO_OVERLAP,
    REMOVE_INTERVAL_INSIDE,
    REMOVE_INTERVAL_LEFT,
    REMOVE_INTERVAL_RIGHT,
    REMOVE_ENTIRE_NODE,
    PROCESSED_CHILDREN
} RemoveState;

// Structure for an interval node
typedef struct IntervalNode {
    int interval[2];        // [start, end]
    char* tag;              // tag name, NULL if no tag
    struct IntervalNode** children;  // array of child nodes
    int numChildren;        // number of children
    int childrenCapacity;   // capacity of children array
} IntervalNode;

// Structure for rehook nodes list
typedef struct {
    IntervalNode** nodes;    // array of nodes to rehook
    int count;              // number of nodes
    int capacity;           // capacity of nodes array
} RehookNodeList;

// Structure for removal result
typedef struct {
    bool removed;
    RemoveState state;
    int remainingInterval[2];
    RehookNodeList rehookNodeList;
} RemoveResult;

// Structure for insertion points
typedef struct {
    int index;
    int start;
    int end;
} InsertPoint;

// Structure for the tree
typedef struct {
    IntervalNode* root;
} TaggedIntervalTree;

// Function prototypes
IntervalNode* createIntervalNode(int start, int end, const char* tag);
void freeIntervalNode(IntervalNode* node);
TaggedIntervalTree* createTaggedIntervalTree(int start, int end);
void freeTaggedIntervalTree(TaggedIntervalTree* tree);
char* intervalNodeToString(IntervalNode* node, int indent);
char* treeToString(TaggedIntervalTree* tree);
int findInsertionPoint(IntervalNode** children, int numChildren, int start);
bool tryMergeWithNeighbors(IntervalNode* node, int newStart, int newEnd, const char* tag);
void addTag(TaggedIntervalTree* tree, const char* tag, int start, int end);
void addTagDFS(IntervalNode* node, const char* tag, int start, int end);
bool removeTag(TaggedIntervalTree* tree, const char* tag, int start, int end);
RemoveResult removeTagDFS(IntervalNode* node, const char* tag, int start, int end);
bool hasTag(TaggedIntervalTree* tree, const char* tag, int start, int end);
bool checkTagDFS(IntervalNode* node, const char* tag, int start, int end);
char* getFormattedText(TaggedIntervalTree* tree, const char* text);

// Helper functions for dynamic arrays
void addChildToNode(IntervalNode* node, IntervalNode* child);
void addNodeToRehookList(RehookNodeList* list, IntervalNode* node);
void freeRehookNodeList(RehookNodeList* list);
RehookNodeList createRehookNodeList();

// Create a new interval node
IntervalNode* createIntervalNode(int start, int end, const char* tag) {
    IntervalNode* node = (IntervalNode*)malloc(sizeof(IntervalNode));
    if (!node) {
        perror("Failed to allocate memory for IntervalNode");
        exit(EXIT_FAILURE);
    }
    
    node->interval[0] = start;
    node->interval[1] = end;
    
    if (tag) {
        node->tag = strdup(tag);
        if (!node->tag) {
            perror("Failed to allocate memory for tag");
            free(node);
            exit(EXIT_FAILURE);
        }
    } else {
        node->tag = NULL;
    }
    
    node->children = NULL;
    node->numChildren = 0;
    node->childrenCapacity = 0;
    
    return node;
}

// Free an interval node and all its children
void freeIntervalNode(IntervalNode* node) {
    if (!node) return;
    
    // Free tag if it exists
    if (node->tag) {
        free(node->tag);
    }
    
    // Free all children
    for (int i = 0; i < node->numChildren; i++) {
        freeIntervalNode(node->children[i]);
    }
    
    // Free children array
    if (node->children) {
        free(node->children);
    }
    
    // Free the node itself
    free(node);
}

// Create a new tagged interval tree
TaggedIntervalTree* createTaggedIntervalTree(int start, int end) {
    TaggedIntervalTree* tree = (TaggedIntervalTree*)malloc(sizeof(TaggedIntervalTree));
    if (!tree) {
        perror("Failed to allocate memory for TaggedIntervalTree");
        exit(EXIT_FAILURE);
    }
    
    tree->root = createIntervalNode(start, end, NULL);
    
    return tree;
}

// Free a tagged interval tree
void freeTaggedIntervalTree(TaggedIntervalTree* tree) {
    if (!tree) return;
    
    freeIntervalNode(tree->root);
    free(tree);
}

// Create a string representation of an interval node
char* intervalNodeToString(IntervalNode* node, int indent) {
    if (!node) return strdup("");
    
    // Calculate buffer size
    int bufferSize = 256;  // Starting size
    for (int i = 0; i < node->numChildren; i++) {
        bufferSize += 256;  // Add space for each child
    }
    
    char* result = (char*)malloc(bufferSize);
    if (!result) {
        perror("Failed to allocate memory for string representation");
        exit(EXIT_FAILURE);
    }
    
    char indentStr[128];
    for (int i = 0; i < indent; i++) {
        indentStr[i] = ' ';
    }
    indentStr[indent] = '\0';
    
    int offset = 0;
    
    // Add this node
    if (node->tag) {
        offset += snprintf(result + offset, bufferSize - offset, 
                          "%s[%d,%d] tag: %s\n", 
                          indentStr, node->interval[0], node->interval[1], node->tag);
    } else {
        offset += snprintf(result + offset, bufferSize - offset, 
                          "%s[%d,%d]\n", 
                          indentStr, node->interval[0], node->interval[1]);
    }
    
    // Add children
    for (int i = 0; i < node->numChildren; i++) {
        char* childStr = intervalNodeToString(node->children[i], indent + 2);
        offset += snprintf(result + offset, bufferSize - offset, "%s", childStr);
        free(childStr);
    }
    
    return result;
}

// Get string representation of the tree
char* treeToString(TaggedIntervalTree* tree) {
    if (!tree || !tree->root) return strdup("");
    return intervalNodeToString(tree->root, 0);
}

// Add a child to a node
void addChildToNode(IntervalNode* node, IntervalNode* child) {
    // Expand capacity if needed
    if (node->numChildren >= node->childrenCapacity) {
        int newCapacity = node->childrenCapacity == 0 ? 4 : node->childrenCapacity * 2;
        IntervalNode** newChildren = (IntervalNode**)realloc(node->children, 
                                    newCapacity * sizeof(IntervalNode*));
        if (!newChildren) {
            perror("Failed to allocate memory for children");
            exit(EXIT_FAILURE);
        }
        
        node->children = newChildren;
        node->childrenCapacity = newCapacity;
    }
    
    // Add child
    node->children[node->numChildren++] = child;
}

// Binary search to find insertion point
int findInsertionPoint(IntervalNode** children, int numChildren, int start) {
    if (numChildren == 0) return 0;
    
    int left = 0;
    int right = numChildren - 1;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        if (children[mid]->interval[0] == start) {
            return mid;
        } else if (children[mid]->interval[0] < start) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return left;
}

// Create a new rehook node list
RehookNodeList createRehookNodeList() {
    RehookNodeList list;
    list.nodes = NULL;
    list.count = 0;
    list.capacity = 0;
    return list;
}

// Add a node to the rehook list
void addNodeToRehookList(RehookNodeList* list, IntervalNode* node) {
    // Expand capacity if needed
    if (list->count >= list->capacity) {
        int newCapacity = list->capacity == 0 ? 4 : list->capacity * 2;
        IntervalNode** newNodes = (IntervalNode**)realloc(list->nodes, 
                                   newCapacity * sizeof(IntervalNode*));
        if (!newNodes) {
            perror("Failed to allocate memory for rehook list");
            exit(EXIT_FAILURE);
        }
        
        list->nodes = newNodes;
        list->capacity = newCapacity;
    }
    
    // Add node
    list->nodes[list->count++] = node;
}

// Free a rehook node list
void freeRehookNodeList(RehookNodeList* list) {
    if (list->nodes) {
        free(list->nodes);
    }
    list->nodes = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Try to merge a new interval with existing children
bool tryMergeWithNeighbors(IntervalNode* node, int newStart, int newEnd, const char* tag) {
    if (node->numChildren == 0) return false;
    
    // Find potential neighbors using binary search
    int index = findInsertionPoint(node->children, node->numChildren, newStart);
    
    // Check left neighbor if exists
    if (index > 0) {
        IntervalNode* leftNeighbor = node->children[index - 1];
        if (leftNeighbor->tag && tag && strcmp(leftNeighbor->tag, tag) == 0 && 
            leftNeighbor->interval[1] >= newStart) {
            // Can merge with left neighbor
            leftNeighbor->interval[1] = leftNeighbor->interval[1] > newEnd ? 
                                       leftNeighbor->interval[1] : newEnd;
            
            // Check if we can also merge with right neighbor
            if (index < node->numChildren) {
                IntervalNode* rightNeighbor = node->children[index];
                if (rightNeighbor->tag && tag && strcmp(rightNeighbor->tag, tag) == 0 && 
                    leftNeighbor->interval[1] >= rightNeighbor->interval[0]) {
                    leftNeighbor->interval[1] = leftNeighbor->interval[1] > rightNeighbor->interval[1] ? 
                                               leftNeighbor->interval[1] : rightNeighbor->interval[1];
                    
                    // Move right neighbor's children to left neighbor
                    for (int i = 0; i < rightNeighbor->numChildren; i++) {
                        addChildToNode(leftNeighbor, rightNeighbor->children[i]);
                    }
                    
                    // Free right neighbor's resources except children
                    if (rightNeighbor->tag) free(rightNeighbor->tag);
                    if (rightNeighbor->children) free(rightNeighbor->children);
                    free(rightNeighbor);
                    
                    // Remove right neighbor from node's children
                    for (int i = index; i < node->numChildren - 1; i++) {
                        node->children[i] = node->children[i + 1];
                    }
                    node->numChildren--;
                }
            }
            return true;
        }
    }
    
    // Check right neighbor if exists
    if (index < node->numChildren) {
        IntervalNode* rightNeighbor = node->children[index];
        if (rightNeighbor->tag && tag && strcmp(rightNeighbor->tag, tag) == 0 && 
            newEnd >= rightNeighbor->interval[0]) {
            // Can merge with right neighbor
            rightNeighbor->interval[0] = rightNeighbor->interval[0] < newStart ? 
                                        rightNeighbor->interval[0] : newStart;
            return true;
        }
    }
    
    return false;
}

// Add a tag to an interval
void addTag(TaggedIntervalTree* tree, const char* tag, int start, int end) {
    if (start >= end) return; // Invalid interval
    
    printf("Adding tag %s to interval [%d,%d]\n", tag, start, end);
    addTagDFS(tree->root, tag, start, end);
}

// DFS helper for adding tags
void addTagDFS(IntervalNode* node, const char* tag, int start, int end) {
    // Make sure we're working within the node's interval
    start = start > node->interval[0] ? start : node->interval[0];
    end = end < node->interval[1] ? end : node->interval[1];
    
    if (start >= end) return; // No valid interval
    
    // If this node has the same tag, we don't need to add it again
    if (node->tag && strcmp(node->tag, tag) == 0) return;
    
    // If no children, create a new child with this tag
    if (node->numChildren == 0) {
        IntervalNode* newNode = createIntervalNode(start, end, tag);
        addChildToNode(node, newNode);
        return;
    }
    
    // Try to merge with existing children first
    if (tryMergeWithNeighbors(node, start, end, tag)) {
        return;
    }
    
    // We need to find where to insert the new tag
    typedef struct {
        int index;
        int start;
        int end;
    } InsertPoint;
    
    InsertPoint* insertPoints = NULL;
    int numInsertPoints = 0;
    int insertPointsCapacity = 0;
    
    int currentPos = start;
    
    // Use binary search to find the first child that might overlap
    int i = findInsertionPoint(node->children, node->numChildren, currentPos);
    if (i > 0 && node->children[i - 1]->interval[1] > currentPos) {
        // If previous child overlaps with our start, adjust i
        i--;
    }
    
    // Check if we need to insert before the first relevant child
    if (i < node->numChildren && currentPos < node->children[i]->interval[0]) {
        // Add insert point
        if (numInsertPoints >= insertPointsCapacity) {
            insertPointsCapacity = insertPointsCapacity == 0 ? 4 : insertPointsCapacity * 2;
            InsertPoint* newPoints = (InsertPoint*)realloc(insertPoints, 
                                      insertPointsCapacity * sizeof(InsertPoint));
            if (!newPoints) {
                perror("Failed to allocate memory for insert points");
                exit(EXIT_FAILURE);
            }
            insertPoints = newPoints;
        }
        
        insertPoints[numInsertPoints].index = i;
        insertPoints[numInsertPoints].start = currentPos;
        insertPoints[numInsertPoints].end = node->children[i]->interval[0] < end ? 
                                           node->children[i]->interval[0] : end;
        numInsertPoints++;
        
        currentPos = node->children[i]->interval[0] < end ? node->children[i]->interval[0] : end;
    }
    
    // Go through relevant children
    while (i < node->numChildren && currentPos < end) {
        IntervalNode* child = node->children[i];
        
        // If current position overlaps with this child
        if (currentPos < child->interval[1]) {
            // Recursively add tag to this child
            addTagDFS(child, tag, currentPos, end);
            currentPos = child->interval[1];
        }
        
        // If there's a gap after this child and before the next
        if (currentPos < end && i + 1 < node->numChildren && 
            currentPos < node->children[i + 1]->interval[0]) {
            // Add insert point
            if (numInsertPoints >= insertPointsCapacity) {
                insertPointsCapacity = insertPointsCapacity == 0 ? 4 : insertPointsCapacity * 2;
                InsertPoint* newPoints = (InsertPoint*)realloc(insertPoints, 
                                          insertPointsCapacity * sizeof(InsertPoint));
                if (!newPoints) {
                    perror("Failed to allocate memory for insert points");
                    exit(EXIT_FAILURE);
                }
                insertPoints = newPoints;
            }
            
            insertPoints[numInsertPoints].index = i + 1;
            insertPoints[numInsertPoints].start = currentPos;
            insertPoints[numInsertPoints].end = node->children[i + 1]->interval[0] < end ? 
                                               node->children[i + 1]->interval[0] : end;
            numInsertPoints++;
            
            currentPos = node->children[i + 1]->interval[0] < end ? 
                         node->children[i + 1]->interval[0] : end;
        }
        
        i++;
    }
    
    // If we still have interval left after all children
    if (currentPos < end) {
        // Add insert point
        if (numInsertPoints >= insertPointsCapacity) {
            insertPointsCapacity = insertPointsCapacity == 0 ? 4 : insertPointsCapacity * 2;
            InsertPoint* newPoints = (InsertPoint*)realloc(insertPoints, 
                                      insertPointsCapacity * sizeof(InsertPoint));
            if (!newPoints) {
                perror("Failed to allocate memory for insert points");
                exit(EXIT_FAILURE);
            }
            insertPoints = newPoints;
        }
        
        insertPoints[numInsertPoints].index = node->numChildren;
        insertPoints[numInsertPoints].start = currentPos;
        insertPoints[numInsertPoints].end = end;
        numInsertPoints++;
    }
    
    // Insert all the new nodes (in reverse order to not mess up indices)
    for (int i = numInsertPoints - 1; i >= 0; i--) {
        InsertPoint point = insertPoints[i];
        
        // Try to merge with neighbors first
        if (!tryMergeWithNeighbors(node, point.start, point.end, tag)) {
            IntervalNode* newNode = createIntervalNode(point.start, point.end, tag);
            
            // Make space in children array
            if (node->numChildren >= node->childrenCapacity) {
                int newCapacity = node->childrenCapacity == 0 ? 4 : node->childrenCapacity * 2;
                IntervalNode** newChildren = (IntervalNode**)realloc(node->children, 
                                            newCapacity * sizeof(IntervalNode*));
                if (!newChildren) {
                    perror("Failed to allocate memory for children");
                    exit(EXIT_FAILURE);
                }
                
                node->children = newChildren;
                node->childrenCapacity = newCapacity;
            }
            
            // Shift elements to make room for insertion
            for (int j = node->numChildren; j > point.index; j--) {
                node->children[j] = node->children[j - 1];
            }
            
            // Insert the new node
            node->children[point.index] = newNode;
            node->numChildren++;
        }
    }
    
    // Free insert points array
    if (insertPoints) {
        free(insertPoints);
    }
}

// Remove a tag from an interval
bool removeTag(TaggedIntervalTree* tree, const char* tag, int start, int end) {
    if (start >= end) return false; // Invalid interval
    
    printf("Removing tag %s from interval [%d,%d]\n", tag, start, end);
    RemoveResult result = removeTagDFS(tree->root, tag, start, end);
    freeRehookNodeList(&result.rehookNodeList);
    return result.removed;
}

// DFS helper for removing tags
RemoveResult removeTagDFS(IntervalNode* node, const char* tag, int start, int end) {
    // Adjust interval to node boundaries
    int effectiveStart = start > node->interval[0] ? start : node->interval[0];
    int effectiveEnd = end < node->interval[1] ? end : node->interval[1];
    
    RemoveResult result;
    result.removed = false;
    result.state = NO_OVERLAP;
    result.remainingInterval[0] = start;
    result.remainingInterval[1] = end;
    result.rehookNodeList = createRehookNodeList();
    
    if (effectiveStart >= effectiveEnd) {
        return result;
    }
    
    // Check if this node has the tag to remove
    if (node->tag && strcmp(node->tag, tag) == 0) {
        int originalStart = node->interval[0];
        int originalEnd = node->interval[1];
        
        // Case 1: Remove-interval is inside a tag (not touching start and end position)
        if (effectiveStart > originalStart && effectiveEnd < originalEnd) {
            // Create separate collections for children
            IntervalNode** beforeNodes = NULL;
            int numBeforeNodes = 0;
            int beforeNodesCapacity = 0;
            
            IntervalNode** insideNodes = NULL;
            int numInsideNodes = 0;
            int insideNodesCapacity = 0;
            
            IntervalNode** afterNodes = NULL;
            int numAfterNodes = 0;
            int afterNodesCapacity = 0;
            
            // Process children based on their position
            for (int i = 0; i < node->numChildren; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[1] <= effectiveStart) {
                    // Child is entirely before removal interval
                    if (numBeforeNodes >= beforeNodesCapacity) {
                        beforeNodesCapacity = beforeNodesCapacity == 0 ? 4 : beforeNodesCapacity * 2;
                        IntervalNode** newArray = (IntervalNode**)realloc(beforeNodes, 
                                                 beforeNodesCapacity * sizeof(IntervalNode*));
                        if (!newArray) {
                            perror("Failed to allocate memory for beforeNodes");
                            exit(EXIT_FAILURE);
                        }
                        beforeNodes = newArray;
                    }
                    beforeNodes[numBeforeNodes++] = child;
                } else if (child->interval[0] >= effectiveEnd) {
                    // Child is entirely after removal interval
                    if (numAfterNodes >= afterNodesCapacity) {
                        afterNodesCapacity = afterNodesCapacity == 0 ? 4 : afterNodesCapacity * 2;
                        IntervalNode** newArray = (IntervalNode**)realloc(afterNodes, 
                                                 afterNodesCapacity * sizeof(IntervalNode*));
                        if (!newArray) {
                            perror("Failed to allocate memory for afterNodes");
                            exit(EXIT_FAILURE);
                        }
                        afterNodes = newArray;
                    }
                    afterNodes[numAfterNodes++] = child;
                } else {
                    // Child overlaps with removal interval - needs further processing
                    RemoveResult childResult = removeTagDFS(child, tag, effectiveStart, effectiveEnd);
                    
                    if (childResult.removed && 
                        (childResult.state == REMOVE_ENTIRE_NODE || childResult.state == REMOVE_INTERVAL_INSIDE)) {
                        // If child is completely removed or split, add its rehook nodes
                        for (int j = 0; j < childResult.rehookNodeList.count; j++) {
                            addNodeToRehookList(&result.rehookNodeList, childResult.rehookNodeList.nodes[j]);
                        }
                        // Clear the rehookNodeList without freeing the nodes
                        childResult.rehookNodeList.count = 0;
                    } else {
                        // If child is partially removed or nothing was removed, keep it
                        if (numInsideNodes >= insideNodesCapacity) {
                            insideNodesCapacity = insideNodesCapacity == 0 ? 4 : insideNodesCapacity * 2;
                            IntervalNode** newArray = (IntervalNode**)realloc(insideNodes, 
                                                     insideNodesCapacity * sizeof(IntervalNode*));
                            if (!newArray) {
                                perror("Failed to allocate memory for insideNodes");
                                exit(EXIT_FAILURE);
                            }
                            insideNodes = newArray;
                        }
                        insideNodes[numInsideNodes++] = child;
                    }
                    
                    // Free the rehookNodeList without freeing the nodes
                    free(childResult.rehookNodeList.nodes);
                }
            }
            
            // Create pre-tag node (before the removal interval)
            if (effectiveStart > originalStart) {
                IntervalNode* preTagNode = createIntervalNode(originalStart, effectiveStart, tag);
                
                // Add before children to pre-tag node
                for (int i = 0; i < numBeforeNodes; i++) {
                    addChildToNode(preTagNode, beforeNodes[i]);
                }
                
                addNodeToRehookList(&result.rehookNodeList, preTagNode);
            } else {
                // If removal starts at node start, just add before nodes to rehook list
                for (int i = 0; i < numBeforeNodes; i++) {
                    addNodeToRehookList(&result.rehookNodeList, beforeNodes[i]);
                }
            }
            
            // Add inside nodes to rehook list
            for (int i = 0; i < numInsideNodes; i++) {
                addNodeToRehookList(&result.rehookNodeList, insideNodes[i]);
            }
            
            // Create post-tag node (after the removal interval)
            if (effectiveEnd < originalEnd) {
                IntervalNode* postTagNode = createIntervalNode(effectiveEnd, originalEnd, tag);
                
                // Add after children to post-tag node
                for (int i = 0; i < numAfterNodes; i++) {
                    addChildToNode(postTagNode, afterNodes[i]);
                }
                
                addNodeToRehookList(&result.rehookNodeList, postTagNode);
            } else {
                // If removal ends at node end, just add after nodes to rehook list
                for (int i = 0; i < numAfterNodes; i++) {
                    addNodeToRehookList(&result.rehookNodeList, afterNodes[i]);
                }
            }
            
            // Free temporary arrays
            if (beforeNodes) free(beforeNodes);
            if (insideNodes) free(insideNodes);
            if (afterNodes) free(afterNodes);
            
            result.removed = true;
            result.state = REMOVE_INTERVAL_INSIDE;
            result.remainingInterval[0] = end;
            result.remainingInterval[1] = end; // Empty interval
            
            return result;
        }
        
        // Case 2: Remove-interval starts at or before tag start but ends within tag
        if (effectiveStart <= originalStart && effectiveEnd < originalEnd) {
            // Adjust this node's interval to start at the end of the removal
            node->interval[0] = effectiveEnd;
            
            // Process any children that might be affected
            int* childrenToRemove = NULL;
            int numChildrenToRemove = 0;
            int childrenToRemoveCapacity = 0;
            
            for (int i = 0; i < node->numChildren; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[1] <= effectiveEnd) {
                    // This child is entirely removed
                    if (numChildrenToRemove >= childrenToRemoveCapacity) {
                        childrenToRemoveCapacity = childrenToRemoveCapacity == 0 ? 4 : childrenToRemoveCapacity * 2;
                        int* newArray = (int*)realloc(childrenToRemove, 
                                       childrenToRemoveCapacity * sizeof(int));
                        if (!newArray) {
                            perror("Failed to allocate memory for childrenToRemove");
                            exit(EXIT_FAILURE);
                        }
                        childrenToRemove = newArray;
                    }
                    childrenToRemove[numChildrenToRemove++] = i;
                    freeIntervalNode(child);
                } else if (child->interval[0] < effectiveEnd) {
                    // This child is partially affected
                    RemoveResult childResult = removeTagDFS(child, tag, effectiveStart, effectiveEnd);
                    
                    if (childResult.removed && childResult.state == REMOVE_ENTIRE_NODE) {
                        // Mark for removal
                        if (numChildrenToRemove >= childrenToRemoveCapacity) {
                            childrenToRemoveCapacity = childrenToRemoveCapacity == 0 ? 4 : childrenToRemoveCapacity * 2;
                            int* newArray = (int*)realloc(childrenToRemove, 
                                           childrenToRemoveCapacity * sizeof(int));
                            if (!newArray) {
                                perror("Failed to allocate memory for childrenToRemove");
                                exit(EXIT_FAILURE);
                            }
                            childrenToRemove = newArray;
                        }
                        childrenToRemove[numChildrenToRemove++] = i;
                        
                        // Add any rehook nodes back to the node's children
                        for (int j = 0; j < childResult.rehookNodeList.count; j++) {
                            IntervalNode* rehookNode = childResult.rehookNodeList.nodes[j];
                            if (rehookNode->interval[0] >= effectiveEnd) {
int insertPos = findInsertionPoint(node->children, node->numChildren, 
                                             rehookNode->interval[0]);
                                
                                // Make space for new node
                                if (node->numChildren >= node->childrenCapacity) {
                                    int newCapacity = node->childrenCapacity == 0 ? 4 : node->childrenCapacity * 2;
                                    IntervalNode** newChildren = (IntervalNode**)realloc(node->children, 
                                                                newCapacity * sizeof(IntervalNode*));
                                    if (!newChildren) {
                                        perror("Failed to allocate memory for children");
                                        exit(EXIT_FAILURE);
                                    }
                                    node->children = newChildren;
                                    node->childrenCapacity = newCapacity;
                                }
                                
                                // Shift elements to make room
                                for (int k = node->numChildren; k > insertPos; k--) {
                                    node->children[k] = node->children[k - 1];
                                }
                                
                                node->children[insertPos] = rehookNode;
                                node->numChildren++;
                            }
                        }
                    }
                    
                    // Free the rehook list structure but not its nodes
                    free(childResult.rehookNodeList.nodes);
                }
            }
            
            // Remove affected children (in reverse order to not mess up indices)
            for (int i = numChildrenToRemove - 1; i >= 0; i--) {
                int index = childrenToRemove[i];
                
                // Shift elements to remove the node
                for (int j = index; j < node->numChildren - 1; j++) {
                    node->children[j] = node->children[j + 1];
                }
                node->numChildren--;
            }
            
            // Free temporary array
            if (childrenToRemove) free(childrenToRemove);
            
            result.removed = true;
            result.state = REMOVE_INTERVAL_LEFT;
            result.remainingInterval[0] = effectiveEnd;
            result.remainingInterval[1] = end;
            
            return result;
        }
        
        // Case 3: Remove-interval starts within tag but extends to or beyond tag end
        if (effectiveStart > originalStart && effectiveEnd >= originalEnd) {
            // Adjust this node's interval to end at the start of the removal
            node->interval[1] = effectiveStart;
            
            // Process any children that might be affected
            int* childrenToRemove = NULL;
            int numChildrenToRemove = 0;
            int childrenToRemoveCapacity = 0;
            
            for (int i = 0; i < node->numChildren; i++) {
                IntervalNode* child = node->children[i];
                
                if (child->interval[0] >= effectiveStart) {
                    // This child is entirely removed
                    if (numChildrenToRemove >= childrenToRemoveCapacity) {
                        childrenToRemoveCapacity = childrenToRemoveCapacity == 0 ? 4 : childrenToRemoveCapacity * 2;
                        int* newArray = (int*)realloc(childrenToRemove, 
                                       childrenToRemoveCapacity * sizeof(int));
                        if (!newArray) {
                            perror("Failed to allocate memory for childrenToRemove");
                            exit(EXIT_FAILURE);
                        }
                        childrenToRemove = newArray;
                    }
                    childrenToRemove[numChildrenToRemove++] = i;
                    freeIntervalNode(child);
                } else if (child->interval[1] > effectiveStart) {
                    // This child is partially affected
                    RemoveResult childResult = removeTagDFS(child, tag, effectiveStart, child->interval[1]);
                    
                    if (childResult.removed && childResult.state == REMOVE_ENTIRE_NODE) {
                        // Mark for removal
                        if (numChildrenToRemove >= childrenToRemoveCapacity) {
                            childrenToRemoveCapacity = childrenToRemoveCapacity == 0 ? 4 : childrenToRemoveCapacity * 2;
                            int* newArray = (int*)realloc(childrenToRemove, 
                                           childrenToRemoveCapacity * sizeof(int));
                            if (!newArray) {
                                perror("Failed to allocate memory for childrenToRemove");
                                exit(EXIT_FAILURE);
                            }
                            childrenToRemove = newArray;
                        }
                        childrenToRemove[numChildrenToRemove++] = i;
                        
                        // Add any rehook nodes back to the node's children
                        for (int j = 0; j < childResult.rehookNodeList.count; j++) {
                            IntervalNode* rehookNode = childResult.rehookNodeList.nodes[j];
                            if (rehookNode->interval[1] <= effectiveStart) {
                                int insertPos = findInsertionPoint(node->children, node->numChildren, 
                                             rehookNode->interval[0]);
                                
                                // Make space for new node
                                if (node->numChildren >= node->childrenCapacity) {
                                    int newCapacity = node->childrenCapacity == 0 ? 4 : node->childrenCapacity * 2;
                                    IntervalNode** newChildren = (IntervalNode**)realloc(node->children, 
                                                                newCapacity * sizeof(IntervalNode*));
                                    if (!newChildren) {
                                        perror("Failed to allocate memory for children");
                                        exit(EXIT_FAILURE);
                                    }
                                    node->children = newChildren;
                                    node->childrenCapacity = newCapacity;
                                }
                                
                                // Shift elements to make room
                                for (int k = node->numChildren; k > insertPos; k--) {
                                    node->children[k] = node->children[k - 1];
                                }
                                
                                node->children[insertPos] = rehookNode;
                                node->numChildren++;
                            }
                        }
                    }
                    
                    // Free the rehook list structure but not its nodes
                    free(childResult.rehookNodeList.nodes);
                }
            }
            
            // Remove affected children (in reverse order to not mess up indices)
            for (int i = numChildrenToRemove - 1; i >= 0; i--) {
                int index = childrenToRemove[i];
                
                // Shift elements to remove the node
                for (int j = index; j < node->numChildren - 1; j++) {
                    node->children[j] = node->children[j + 1];
                }
                node->numChildren--;
            }
            
            // Free temporary array
            if (childrenToRemove) free(childrenToRemove);
            
            result.removed = true;
            result.state = REMOVE_INTERVAL_RIGHT;
            result.remainingInterval[0] = start;
            result.remainingInterval[1] = effectiveStart;
            
            return result;
        }
        
        // Case 4: Remove-interval completely covers tag
        if (effectiveStart <= originalStart && effectiveEnd >= originalEnd) {
            // Process children to see if any need tag removal too
            for (int i = 0; i < node->numChildren; i++) {
                IntervalNode* child = node->children[i];
                
                // If child overlaps with removal interval
                if (child->interval[0] < effectiveEnd && child->interval[1] > effectiveStart) {
                    RemoveResult childResult = removeTagDFS(child, tag, effectiveStart, effectiveEnd);
                    
                    if (childResult.removed) {
                        // Add rehook nodes from child
                        for (int j = 0; j < childResult.rehookNodeList.count; j++) {
                            addNodeToRehookList(&result.rehookNodeList, childResult.rehookNodeList.nodes[j]);
                        }
                        // Clear without freeing nodes
                        childResult.rehookNodeList.count = 0;
                    } else {
                        // Keep child as is
                        addNodeToRehookList(&result.rehookNodeList, child);
                    }
                    
                    // Free the rehook list structure but not its nodes
                    free(childResult.rehookNodeList.nodes);
                } else {
                    // Child doesn't overlap, keep it
                    addNodeToRehookList(&result.rehookNodeList, child);
                }
            }
            
            // Clear node's children without freeing them
            node->numChildren = 0;
            
            result.removed = true;
            result.state = REMOVE_ENTIRE_NODE;
            result.remainingInterval[0] = effectiveEnd;
            result.remainingInterval[1] = end;
            
            return result;
        }
    }
    
    // This node doesn't have the tag to remove, so process children
    bool removed = false;
    
    // Use binary search to find children that might overlap with the removal interval
    int startIdx = findInsertionPoint(node->children, node->numChildren, effectiveStart);
    if (startIdx > 0 && node->children[startIdx - 1]->interval[1] > effectiveStart) {
        startIdx--;
    }
    
    int i = startIdx;
    while (i < node->numChildren) {
        IntervalNode* child = node->children[i];
        
        // Skip if no overlap
        if (effectiveEnd <= child->interval[0] || effectiveStart >= child->interval[1]) {
            i++;
            continue;
        }
        
        RemoveResult childResult = removeTagDFS(child, tag, start, end);
        
        if (childResult.removed) {
            removed = true;
            
            if (childResult.state == REMOVE_ENTIRE_NODE || 
                childResult.state == REMOVE_INTERVAL_INSIDE) {
                // Remove this child
                for (int j = i; j < node->numChildren - 1; j++) {
                    node->children[j] = node->children[j + 1];
                }
                node->numChildren--;
                
                // Insert rehook nodes at the right positions
                for (int j = 0; j < childResult.rehookNodeList.count; j++) {
                    IntervalNode* rehookNode = childResult.rehookNodeList.nodes[j];
                    int insertPos = findInsertionPoint(node->children, node->numChildren, rehookNode->interval[0]);
                    
                    // Make space for new node
                    if (node->numChildren >= node->childrenCapacity) {
                        int newCapacity = node->childrenCapacity == 0 ? 4 : node->childrenCapacity * 2;
                        IntervalNode** newChildren = (IntervalNode**)realloc(node->children, 
                                                    newCapacity * sizeof(IntervalNode*));
                        if (!newChildren) {
                            perror("Failed to allocate memory for children");
                            exit(EXIT_FAILURE);
                        }
                        node->children = newChildren;
                        node->childrenCapacity = newCapacity;
                    }
                    
                    // Shift elements to make room
                    for (int k = node->numChildren; k > insertPos; k--) {
                        node->children[k] = node->children[k - 1];
                    }
                    
                    node->children[insertPos] = rehookNode;
                    node->numChildren++;
                }
                
                // Update position for next iteration
                i = findInsertionPoint(node->children, node->numChildren, effectiveStart);
                if (i > 0 && node->children[i - 1]->interval[1] > effectiveStart) {
                    i--;
                }
            } else {
                // For LEFT and RIGHT states, the child was adjusted, so keep it
                i++;
            }
            
            // Continue with remaining interval if any
            if (childResult.remainingInterval[0] < childResult.remainingInterval[1]) {
                // Call recursively with remaining interval
                RemoveResult remainingResult = removeTagDFS(
                    node, 
                    tag, 
                    childResult.remainingInterval[0], 
                    childResult.remainingInterval[1]
                );
                
                if (remainingResult.removed) {
                    removed = true;
                }
                
                // Free the rehook list structure
                free(remainingResult.rehookNodeList.nodes);
            }
        } else {
            i++;
        }
        
        // Free the rehook list structure but not its nodes
        free(childResult.rehookNodeList.nodes);
    }
    
    // Ensure child intervals are properly nested within parent
    for (int i = 0; i < node->numChildren; i++) {
        if (node->children[i]->interval[0] < node->interval[0]) {
            node->children[i]->interval[0] = node->interval[0];
        }
        if (node->children[i]->interval[1] > node->interval[1]) {
            node->children[i]->interval[1] = node->interval[1];
        }
    }
    
    result.removed = removed;
    result.state = PROCESSED_CHILDREN;
    result.remainingInterval[0] = effectiveEnd > start ? effectiveEnd : start;
    result.remainingInterval[1] = end;
    
    return result;
}

// Check if an interval has a specific tag
bool hasTag(TaggedIntervalTree* tree, const char* tag, int start, int end) {
    return checkTagDFS(tree->root, tag, start, end);
}

// DFS helper for checking tags
bool checkTagDFS(IntervalNode* node, const char* tag, int start, int end) {
    // If this node has the tag and fully contains the interval
    if (node->tag && strcmp(node->tag, tag) == 0 && 
        node->interval[0] <= start && 
        node->interval[1] >= end) {
        return true;
    }
    
    // Use binary search to find children that might overlap
    int i = findInsertionPoint(node->children, node->numChildren, start);
    if (i > 0 && node->children[i - 1]->interval[1] > start) {
        i--;
    }
    
    // Check relevant children
    while (i < node->numChildren) {
        IntervalNode* child = node->children[i];
        
        // Skip if no overlap
        if (end <= child->interval[0] || start >= child->interval[1]) {
            i++;
            continue;
        }
        
        if (checkTagDFS(child, tag, start, end)) {
            return true;
        }
        
        i++;
    }
    
    return false;
}

// Structure for tag markers
typedef struct {
    int position;
    char* tag;
    bool isOpening;
} TagMarker;

// Compare function for sorting markers
int compareMarkers(const void* a, const void* b) {
    const TagMarker* markerA = (const TagMarker*)a;
    const TagMarker* markerB = (const TagMarker*)b;
    
    if (markerA->position == markerB->position) {
        return markerA->isOpening ? 1 : -1;
    }
    return markerA->position - markerB->position;
}

// Get formatted text with tags
char* getFormattedText(TaggedIntervalTree* tree, const char* text) {
    if (!tree || !text) return NULL;
    
    // Count maximum possible markers (2 per node)
    int maxMarkers = 0;
    
    void countNodes(IntervalNode* node) {
        if (node->tag) {
            maxMarkers += 2;  // Opening and closing tag
        }
        
        for (int i = 0; i < node->numChildren; i++) {
            countNodes(node->children[i]);
        }
    }
    
    countNodes(tree->root);
    
    // Allocate markers array
    TagMarker* markers = (TagMarker*)malloc(maxMarkers * sizeof(TagMarker));
    if (!markers) {
        perror("Failed to allocate memory for markers");
        return NULL;
    }
    
    int markerCount = 0;
    
    // Collect markers
    void collectMarkers(IntervalNode* node) {
        if (node->tag) {
            // Add opening tag
            markers[markerCount].position = node->interval[0];
            markers[markerCount].tag = strdup(node->tag);
            markers[markerCount].isOpening = true;
            markerCount++;
            
            // Add closing tag
            markers[markerCount].position = node->interval[1];
            markers[markerCount].tag = strdup(node->tag);
            markers[markerCount].isOpening = false;
            markerCount++;
        }
        
        for (int i = 0; i < node->numChildren; i++) {
            collectMarkers(node->children[i]);
        }
    }
    
    collectMarkers(tree->root);
    
    // Sort markers by position (closing tags come before opening tags at same position)
    qsort(markers, markerCount, sizeof(TagMarker), compareMarkers);
    
    // Calculate result size
    int textLen = strlen(text);
    int resultSize = textLen + 1;  // Start with text length + null terminator
    
    for (int i = 0; i < markerCount; i++) {
        // Add space for <tag> or </tag>
        resultSize += strlen(markers[i].tag) + (markers[i].isOpening ? 2 : 3);
    }
    
    // Allocate result buffer
    char* result = (char*)malloc(resultSize * sizeof(char));
    if (!result) {
        perror("Failed to allocate memory for formatted text");
        
        // Free markers
        for (int i = 0; i < markerCount; i++) {
            free(markers[i].tag);
        }
        free(markers);
        
        return NULL;
    }
    
    // Insert tags into text
    int resultPos = 0;
    int lastPosition = 0;
    
    for (int i = 0; i < markerCount; i++) {
        // Add text up to this marker
        int textToAdd = markers[i].position - lastPosition;
        if (textToAdd > 0) {
            strncpy(result + resultPos, text + lastPosition, textToAdd);
            resultPos += textToAdd;
        }
        lastPosition = markers[i].position;
        
        // Add tag
        if (markers[i].isOpening) {
            result[resultPos++] = '<';
            strcpy(result + resultPos, markers[i].tag);
            resultPos += strlen(markers[i].tag);
            result[resultPos++] = '>';
        } else {
            result[resultPos++] = '<';
            result[resultPos++] = '/';
            strcpy(result + resultPos, markers[i].tag);
            resultPos += strlen(markers[i].tag);
            result[resultPos++] = '>';
        }
    }
    
    // Add remaining text
    strcpy(result + resultPos, text + lastPosition);
    
    // Free markers
    for (int i = 0; i < markerCount; i++) {
        free(markers[i].tag);
    }
    free(markers);
    
    return result;
}

// Example of usage
int main() {
    // Create a new tree with text range [0, 20]
    TaggedIntervalTree* tree = createTaggedIntervalTree(0, 20);
    
    // Add some tags
    addTag(tree, "b", 2, 10);
    addTag(tree, "i", 5, 15);
    addTag(tree, "u", 8, 12);
    
    // Print tree structure
    char* treeStr = treeToString(tree);
    printf("Tree after adding tags:\n%s\n", treeStr);
    free(treeStr);
    
    // Remove a tag
    removeTag(tree, "i", 7, 10);
    
    // Print updated tree
    treeStr = treeToString(tree);
    printf("Tree after removing i tag from [7,10]:\n%s\n", treeStr);
    free(treeStr);
    
    // Check if interval has a tag
    printf("Interval [3,8] has b tag: %s\n", hasTag(tree, "b", 3, 8) ? "true" : "false");
    printf("Interval [11,14] has i tag: %s\n", hasTag(tree, "i", 11, 14) ? "true" : "false");
    
    // Format some text
    const char* text = "0123456789abcdefghij";
    char* formattedText = getFormattedText(tree, text);
    printf("Formatted text: %s\n", formattedText);
    free(formattedText);
    
    // Free tree
    freeTaggedIntervalTree(tree);
    
    return 0;
}
