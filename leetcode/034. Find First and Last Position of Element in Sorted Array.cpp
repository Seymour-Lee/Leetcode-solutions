class Solution {
public:
    vector<int> searchRange(vector<int>& nums, int target) {
        if(nums.size() == 0) return {-1, -1};
        int l = 0, r = nums.size()-1;
        int m;
        while(l <= r){
            m = l + (r - l) / 2;
            if(nums[m] == target) break;
            if(nums[m] < target) l = m + 1;
            else r = m - 1;
        }
        if(l > r) return {-1, -1};
        l = m, r = m;
        while(l > 0 && nums[l-1] == target) l--;
        while(r < nums.size()-1 && nums[r+1] == target) r++;
        return {l, r};
    }
};