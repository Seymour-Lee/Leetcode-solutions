/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *     int val;
 *     TreeNode *left;
 *     TreeNode *right;
 *     TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */
class Solution {
public:
    int diameterOfBinaryTree(TreeNode* root) {
        if(root == nullptr) return 0;
        int result = 0;
        dfs(root, result);
        return result;
    }
    
private:
    int dfs(TreeNode *root, int &result){
        if(root == nullptr) return -1;
        int l = 1 + dfs(root->left, result);
        int r = 1 + dfs(root->right, result);
        result = max(result, l+r);
        return max(l, r);
    }
};