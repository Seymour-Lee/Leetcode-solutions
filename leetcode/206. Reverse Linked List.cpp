/**
 * Definition for singly-linked list.
 * struct ListNode {
 *     int val;
 *     ListNode *next;
 *     ListNode(int x) : val(x), next(NULL) {}
 * };
 */
class Solution {
public:
    ListNode* reverseList(ListNode* head) {
        if(head == nullptr) return head;
        ListNode *dummy = new ListNode(-1);
        
        ListNode *p;
        while(head){
            p = dummy->next;
            dummy->next = head;
            head = head->next;
            dummy->next->next = p;
        }
        
        return dummy->next;
    }
};