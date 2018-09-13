class Solution {
public:
    int coinChange(vector<int>& coins, int amount) {
        if(amount <= 0) return 0;
        vector<int> dp(amount+1, amount+1);
        dp[0] = 0;
        for(int i = 1; i <= amount; i++){
            for(auto coin: coins){
                if(coin <= i){
                    dp[i] = min(dp[i], dp[i-coin]+1);
                }
            }
        }
        return dp.back() > amount? -1: dp.back();
    }
};