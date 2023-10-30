#include "geometry.h"

#include <stdlib.h>

#include <algorithm>

#include "memory.h"

namespace fallout {

// 0x51DEF4
static RectListNode* _rectList = nullptr;

// 0x4C6900
void _GNW_rect_exit()
{
    while (_rectList != nullptr) {
        RectListNode* next = _rectList->next;
        internal_free(_rectList);
        _rectList = next;
    }
}

// 0x4C6924
void _rect_clip_list(RectListNode** rectListNodePtr, Rect* rect)
{
    Rect v1;
    rectCopy(&v1, rect);

    // NOTE: Original code is slightly different.
    while (*rectListNodePtr != nullptr) {
        RectListNode* rectListNode = *rectListNodePtr;
        if (v1.right >= rectListNode->rect.left
            && v1.bottom >= rectListNode->rect.top
            && v1.left <= rectListNode->rect.right
            && v1.top <= rectListNode->rect.bottom) {
            Rect v2;
            rectCopy(&v2, &(rectListNode->rect));

            *rectListNodePtr = rectListNode->next;

            rectListNode->next = _rectList;
            _rectList = rectListNode;

            if (v2.top < v1.top) {
                RectListNode* newRectListNode = _rect_malloc();
                if (newRectListNode == nullptr) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &v2);
                newRectListNode->rect.bottom = v1.top - 1;
                newRectListNode->next = *rectListNodePtr;

                *rectListNodePtr = newRectListNode;
                rectListNodePtr = &(newRectListNode->next);

                v2.top = v1.top;
            }

            if (v2.bottom > v1.bottom) {
                RectListNode* newRectListNode = _rect_malloc();
                if (newRectListNode == nullptr) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &v2);
                newRectListNode->rect.top = v1.bottom + 1;
                newRectListNode->next = *rectListNodePtr;

                *rectListNodePtr = newRectListNode;
                rectListNodePtr = &(newRectListNode->next);

                v2.bottom = v1.bottom;
            }

            if (v2.left < v1.left) {
                RectListNode* newRectListNode = _rect_malloc();
                if (newRectListNode == nullptr) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &v2);
                newRectListNode->rect.right = v1.left - 1;
                newRectListNode->next = *rectListNodePtr;

                *rectListNodePtr = newRectListNode;
                rectListNodePtr = &(newRectListNode->next);
            }

            if (v2.right > v1.right) {
                RectListNode* newRectListNode = _rect_malloc();
                if (newRectListNode == nullptr) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &v2);
                newRectListNode->rect.left = v1.right + 1;
                newRectListNode->next = *rectListNodePtr;

                *rectListNodePtr = newRectListNode;
                rectListNodePtr = &(newRectListNode->next);
            }
        } else {
            rectListNodePtr = &(rectListNode->next);
        }
    }
}

// 0x4C6AAC
RectListNode* rect_clip(Rect* b, Rect* t)
{
    RectListNode* list = nullptr;
    Rect clipped_t;

    if (rectIntersection(t, b, &clipped_t) == 0) {
        RectListNode** next = &list;
        Rect clipped_b[4];
        int k;

        clipped_b[0].left = b->left;
        clipped_b[0].top = b->top;
        clipped_b[0].right = b->right;
        clipped_b[0].bottom = clipped_t.top - 1;

        clipped_b[1].left = b->left;
        clipped_b[1].top = clipped_t.top;
        clipped_b[1].right = clipped_t.left - 1;
        clipped_b[1].bottom = clipped_t.bottom;

        clipped_b[2].left = clipped_t.right + 1;
        clipped_b[2].top = clipped_t.top;
        clipped_b[2].right = b->right;
        clipped_b[2].bottom = clipped_t.bottom;

        clipped_b[3].left = b->left;
        clipped_b[3].top = clipped_t.bottom + 1;
        clipped_b[3].right = b->right;
        clipped_b[3].bottom = b->bottom;

        for (k = 0; k < 4; k++) {
            if (clipped_b[k].left <= clipped_b[k].right && clipped_b[k].top <= clipped_b[k].bottom) {
                *next = _rect_malloc();
                if (*next == nullptr) {
                    return nullptr;
                }

                (*next)->rect = clipped_b[k];
                (*next)->next = nullptr;
                next = &((*next)->next);
            }
        }
    } else {
        list = _rect_malloc();
        if (list == nullptr) {
            return nullptr;
        }

        list->rect = *b;
        list->next = nullptr;
    }

    return list;
}

// 0x4C6BB8
RectListNode* _rect_malloc()
{
    if (_rectList == nullptr) {
        for (int index = 0; index < 10; index++) {
            RectListNode* rectListNode = (RectListNode*)internal_malloc(sizeof(*rectListNode));
            if (rectListNode == nullptr) {
                break;
            }

            // NOTE: Uninline.
            _rect_free(rectListNode);
        }
    }

    if (_rectList == nullptr) {
        return nullptr;
    }

    RectListNode* rectListNode = _rectList;
    _rectList = _rectList->next;

    return rectListNode;
}

// 0x4C6C04
void _rect_free(RectListNode* rectListNode)
{
    rectListNode->next = _rectList;
    _rectList = rectListNode;
}

// Calculates a union of two source rectangles and places it into result
// rectangle.
//
// 0x4C6C18
void rectUnion(const Rect* s1, const Rect* s2, Rect* r)
{
    r->left = std::min(s1->left, s2->left);
    r->top = std::min(s1->top, s2->top);
    r->right = std::max(s1->right, s2->right);
    r->bottom = std::max(s1->bottom, s2->bottom);
}

// Calculates intersection of two source rectangles and places it into third
// rectangle and returns 0. If two source rectangles do not have intersection
// it returns -1 and resulting rectangle is a copy of s1.
//
// 0x4C6C68
int rectIntersection(const Rect* s1, const Rect* s2, Rect* r)
{
    r->left = s1->left;
    r->top = s1->top;
    r->right = s1->right;
    r->bottom = s1->bottom;

    if (s1->left <= s2->right && s2->left <= s1->right && s2->bottom >= s1->top && s2->top <= s1->bottom) {
        if (s2->left > s1->left) {
            r->left = s2->left;
        }

        if (s2->right < s1->right) {
            r->right = s2->right;
        }

        if (s2->top > s1->top) {
            r->top = s2->top;
        }

        if (s2->bottom < s1->bottom) {
            r->bottom = s2->bottom;
        }

        return 0;
    }

    return -1;
}

} // namespace fallout
