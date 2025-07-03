#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <list>
#include <memory>
#include <numeric>
#include <algorithm>
#include <iomanip> // For std::setw, std::left

// --- Enums and Type Aliases ---
enum class OrderType
{
    GoodTillCancel,
    FillandKill
};

enum class Side
{
    Buy,
    Sell
};

using Price = std::int32_t;
using Quantity = std::int32_t;
using OrderId = std::int64_t;

// --- LevelInfo and LevelInfos ---
struct LevelInfo
{
    Price price_;
    Quantity quantity_;

};

using LevelInfos = std::vector<LevelInfo>;

// --- OrderbookLevelInfos Class ---
class OrderbookLevelInfos
{
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_{ bids }
        , asks_{ asks }
    {}

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

// --- Order Class ---
class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price,Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , initialQuantity_{ quantity }
        , remainingQuantity_{ quantity }
    {}

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return initialQuantity_ - remainingQuantity_; }
    bool IsFilled() const {return GetRemainingQuantity()==0;}
    void Fill(Quantity quantity)
    {
        if (quantity > remainingQuantity_) {
            throw std::logic_error("Order (" + std::to_string(GetOrderId()) + ") cannot be filled for more than remaining quantity.");
        }
        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

// --- OrderModify Class ---
class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , quantity_{ quantity }
    {}

    OrderId GetOrderId() const { return orderId_; }
    Price GetPrice() const { return price_; }
    Side GetSide() const { return side_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

// --- TradeInfo and Trade Class ---
struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_{ bidTrade }
        , askTrade_{ askTrade }
    {}
    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

// --- Orderbook Class ---
class Orderbook
{
private:

    struct OrderEntry
    {
        OrderPointer order_{ nullptr};
        OrderPointers::iterator location_;

    };

    std:: map<Price,OrderPointers,std::greater<Price>> bids_;
    std:: map<Price, OrderPointers,std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price)const
    {
        if(side==Side::Buy)
        {
            if(asks_.empty())
            {
                return false;
            }
            const auto& bestAsk = asks_.begin()->first;
            return price >= bestAsk;
        }
        else{ // Side::Sell
            if(bids_.empty())
            {
                return false;
            }
            const auto& bestBid = bids_.begin()->first;
            return price <= bestBid;
        }
    }

    Trades MatchOrders()
    {
        Trades trades;
        trades.reserve(orders_.size()); // Pre-allocate memory as a heuristic

        while(true)
        {
            if(bids_.empty() || asks_.empty()){
                break;
            }

            auto bidEntry = *bids_.begin();
            auto askEntry = *asks_.begin();

            Price bidPrice = bidEntry.first;
            OrderPointers& bids = bidEntry.second;

            Price askPrice = askEntry.first;
            OrderPointers& asks = askEntry.second;

            if(bidPrice < askPrice){ // No overlap between best bid and best ask
                break;
            }

            // Match orders at the current best bid/ask prices
            while(!bids.empty() && !asks.empty())
            {
                auto& bid = bids.front();
                auto& ask = asks.front();

                Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
                bid->Fill(quantity);
                ask->Fill(quantity);

                // Record the trade
                trades.push_back(Trade{
                    TradeInfo{bid->GetOrderId(), bidPrice, quantity}, // Use bidPrice for bid trade
                    TradeInfo{ask->GetOrderId(), askPrice, quantity}  // Use askPrice for ask trade
                });

                if(bid->IsFilled())
                {
                    bids.pop_front();
                    orders_.erase(bid->GetOrderId());
                }
                if(ask->IsFilled())
                {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }
            }

            // Clean up empty price levels
            if(bids.empty()){
                bids_.erase(bidPrice);
            }
            if(asks.empty()){
                asks_.erase(askPrice);
            }
        }

        // Handle FillAndKill orders that could not be fully filled
        if(!bids_.empty())
        {
            auto bidEntry = *bids_.begin();
            OrderPointers& bids = bidEntry.second;
            if(!bids.empty()){ // Check if list is not empty after potential matching
                auto& order = bids.front();
                if(order->GetOrderType() == OrderType::FillandKill)
                {
                    // This FAK order couldn't be fully matched, so cancel it
                    CancelOrder(order->GetOrderId());
                }
            }
        }
        if(!asks_.empty())
        {
            auto askEntry = *asks_.begin();
            OrderPointers& asks = askEntry.second;
            if(!asks.empty()){ // Check if list is not empty after potential matching
                auto& order = asks.front();
                if(order->GetOrderType() == OrderType::FillandKill)
                {
                    // This FAK order couldn't be fully matched, so cancel it
                    CancelOrder(order->GetOrderId());
                }
            }
        }
        return trades;
    }

public:
    Trades AddOrder(OrderPointer order)
    {
        if(orders_.count(order->GetOrderId()))
        {
            // Order with this ID already exists
            std::cout << "Error: Order with ID " << order->GetOrderId() << " already exists. Cannot add duplicate." << std::endl;
            return {};
        }

        // FillAndKill orders are rejected if they cannot match immediately
        if(order->GetOrderType()== OrderType::FillandKill && !CanMatch(order->GetSide(),order->GetPrice())){
            std::cout << "Order " << order->GetOrderId() << " (FAK) rejected: No immediate match available." << std::endl;
            return {};
        }

        OrderPointers::iterator iterator;

        if(order->GetSide() == Side::Buy){
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            // Get iterator to the newly added element (last in list)
            iterator = std::prev(orders.end());
        }
        else{ // Side::Sell
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            // Get iterator to the newly added element (last in list)
            iterator = std::prev(orders.end());
        }

        orders_.insert({order->GetOrderId(),OrderEntry{order,iterator}});
        std::cout << "Added Order: ID " << order->GetOrderId()
                  << ", Side: " << (order->GetSide() == Side::Buy ? "Buy" : "Sell")
                  << ", Price: " << order->GetPrice()
                  << ", Quantity: " << order->GetInitialQuantity()
                  << ", Type: " << (order->GetOrderType() == OrderType::GoodTillCancel ? "GTC" : "FAK") << std::endl;
        return MatchOrders();
    }

    void CancelOrder(OrderId orderId)
    {
        if(!orders_.count(orderId)){
            std::cout << "Error: Order with ID " << orderId << " not found for cancellation." << std::endl;
            return;
        }
        
        auto orderEntry = orders_.at(orderId);
        OrderPointer order = orderEntry.order_;
        OrderPointers::iterator orderIterator = orderEntry.location_;

        orders_.erase(orderId); // Remove from overall orders map

        if(order->GetSide()==Side::Sell){
            auto price = order->GetPrice();
            auto& orders = asks_.at(price);
            orders.erase(orderIterator); // Remove from price level list
            if(orders.empty()){ // If price level becomes empty, remove it from map
                asks_.erase(price);
            }
        }
        else{ // Side::Buy
            auto price = order->GetPrice();
            auto& orders = bids_.at(price);
            orders.erase(orderIterator); // Remove from price level list
            if(orders.empty()){ // If price level becomes empty, remove it from map
                bids_.erase(price);
            }
        }
        std::cout << "Cancelled Order: ID " << orderId << std::endl;
    }

    Trades ModifyOrder(OrderModify orderModify)
    {
        if(!orders_.count(orderModify.GetOrderId())){
            std::cout << "Error: Order with ID " << orderModify.GetOrderId() << " not found for modification." << std::endl;
            return{};
        }
        
        auto orderEntry = orders_.at(orderModify.GetOrderId());
        OrderPointer existingOrder = orderEntry.order_;
        OrderType originalOrderType = existingOrder->GetOrderType(); // Preserve original order type

        std::cout << "Modifying Order ID " << orderModify.GetOrderId()
                  << " from Price: " << existingOrder->GetPrice() << ", Qty: " << existingOrder->GetRemainingQuantity()
                  << " to Price: " << orderModify.GetPrice() << ", Qty: " << orderModify.GetQuantity() << std::endl;

        CancelOrder(orderModify.GetOrderId()); // Cancel existing order
        // Add a new order with the modified details, retaining the original order type
        return AddOrder(orderModify.ToOrderPointer(originalOrderType));
    }

    std::size_t Size() const { return orders_.size();}

    OrderbookLevelInfos GetOrderInfos()const
    {
        LevelInfos bidInfos,askInfos;
        bidInfos.reserve(orders_.size()); // Reserve approximate space
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price,const OrderPointers& orders)
        {
            return LevelInfo{price,std::accumulate(orders.begin(),orders.end(),(Quantity)0,[](std::size_t runningSum,const OrderPointer& order){return runningSum+order->GetRemainingQuantity();})};
        };

        for(const auto& pair : bids_){
            bidInfos.push_back(CreateLevelInfos(pair.first,pair.second));
        }
        for(const auto& pair : asks_){
            askInfos.push_back(CreateLevelInfos(pair.first,pair.second));
        }
        return OrderbookLevelInfos{bidInfos,askInfos};
    }
};

// --- Helper Functions for Main Simulation ---

void PrintOrderbook(const Orderbook& orderbook) {
    OrderbookLevelInfos infos = orderbook.GetOrderInfos();
    std::cout << "\n--- Orderbook Snapshot (Size: " << orderbook.Size() << ") ---" << std::endl;

    std::cout << "Bids:" << std::endl;
    if (infos.GetBids().empty()) {
        std::cout << "  (Empty)" << std::endl;
    } else {
        std::cout << std::left << std::setw(10) << "Price" << "Quantity" << std::endl;
        for (const auto& level : infos.GetBids()) {
            std::cout << std::left << std::setw(10) << level.price_ << level.quantity_ << std::endl;
        }
    }

    std::cout << "Asks:" << std::endl;
    if (infos.GetAsks().empty()) {
        std::cout << "  (Empty)" << std::endl;
    } else {
        std::cout << std::left << std::setw(10) << "Price" << "Quantity" << std::endl;
        for (const auto& level : infos.GetAsks()) {
            std::cout << std::left << std::setw(10) << level.price_ << level.quantity_ << std::endl;
        }
    }
    std::cout << "--------------------------------------" << std::endl;
}

void PrintTrades(const Trades& trades) {
    if (trades.empty()) {
        std::cout << "No trades occurred." << std::endl;
        return;
    }
    std::cout << "\n--- Trades Executed ---" << std::endl;
    std::cout << std::left << std::setw(15) << "Bid Order ID"
              << std::left << std::setw(10) << "Bid Price"
              << std::left << std::setw(10) << "Ask Order ID"
              << std::left << std::setw(10) << "Ask Price"
              << std::left << "Quantity" << std::endl;
    for (const auto& trade : trades) {
        std::cout << std::left << std::setw(15) << trade.GetBidTrade().orderId_
                  << std::left << std::setw(10) << trade.GetBidTrade().price_
                  << std::left << std::setw(10) << trade.GetAskTrade().orderId_
                  << std::left << std::setw(10) << trade.GetAskTrade().price_
                  << std::left << trade.GetBidTrade().quantity_ << std::endl;
    }
    std::cout << "-------------------------" << std::endl;
}

// --- Main Simulation Logic ---
int main(){
    Orderbook orderbook;
    Trades trades;
    OrderId nextOrderId = 1;

    std::cout << "--- C++14 Orderbook Simulation ---" << std::endl;

    // --- Scenario 1: Adding GTC Orders ---
    std::cout << "\n--- Scenario 1: Adding GTC Orders ---" << std::endl;
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Buy, 100, 50)); // ID 1
    PrintTrades(trades);
    PrintOrderbook(orderbook);

    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Buy, 99, 100)); // ID 2
    PrintTrades(trades);
    PrintOrderbook(orderbook);

    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Sell, 102, 70)); // ID 3
    PrintTrades(trades);
    PrintOrderbook(orderbook);

    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Sell, 101, 30)); // ID 4
    PrintTrades(trades);
    PrintOrderbook(orderbook);

    // --- Scenario 2: Basic Matching (GTC) ---
    std::cout << "\n--- Scenario 2: Basic Matching (GTC) ---" << std::endl;
    // This order should match with Ask ID 4 (Price 101, Qty 30)
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Buy, 101, 40)); // ID 5
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Bid 5 (10 remaining) should now be at 101. Ask ID 4 gone.

    // This order should match with Bid ID 1 (Price 100, Qty 50)
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Sell, 100, 20)); // ID 6
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Bid ID 1 (30 remaining) should be at 100.

    // This order should match remaining Bid ID 1 (Price 100, Qty 30)
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Sell, 99, 30)); // ID 7
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Bid ID 1 should be gone. Bid ID 5 (10 remaining) still at 101.

    // --- Scenario 3: Fill and Kill (FAK) Orders ---
    std::cout << "\n--- Scenario 3: Fill and Kill (FAK) Orders ---" << std::endl;
    // FAK Buy Order - should partially fill with Ask ID 3 (remaining 70), and then kill remainder
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::FillandKill, nextOrderId++, Side::Buy, 102, 80)); // ID 8
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Ask ID 3 should be gone.

    // FAK Sell Order - should partially fill with Bid ID 5 (remaining 10), then kill remainder
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::FillandKill, nextOrderId++, Side::Sell, 100, 20)); // ID 9
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Bid ID 5 should be gone.

    // FAK Buy Order - no immediate match, should be rejected/killed immediately upon add
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::FillandKill, nextOrderId++, Side::Buy, 98, 10)); // ID 10
    PrintTrades(trades); // Should show no trades
    PrintOrderbook(orderbook); // Order 10 should not appear in the book

    // --- Scenario 4: Order Cancellation ---
    std::cout << "\n--- Scenario 4: Order Cancellation ---" << std::endl;
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Buy, 97, 25)); // ID 11
    PrintTrades(trades);
    PrintOrderbook(orderbook);

    orderbook.CancelOrder(11); // Cancel order 11
    PrintOrderbook(orderbook);

    orderbook.CancelOrder(999); // Attempt to cancel non-existent order
    PrintOrderbook(orderbook);

    // --- Scenario 5: Order Modification ---
    std::cout << "\n--- Scenario 5: Order Modification ---" << std::endl;
    trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, nextOrderId++, Side::Buy, 95, 60)); // ID 12
    PrintTrades(trades);
    PrintOrderbook(orderbook);

    // Modify Order 12: change price and quantity
    trades = orderbook.ModifyOrder(OrderModify(12, Side::Buy, 96, 75));
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Order 12 should now be at price 96 with qty 75

    // Modify Order 2: change price and quantity (from 99, 100 to 100, 10)
    // This should cause a match!
    trades = orderbook.ModifyOrder(OrderModify(2, Side::Buy, 100, 10));
    PrintTrades(trades);
    PrintOrderbook(orderbook); // Order 2 should be gone, matched with Ask ID 6 (price 100, Qty 20)
                               // Oh wait, Ask ID 6 was already partially filled. Let's trace it.
                               // After scenario 2, Ask ID 6 (qty 20) was added and matched Bid ID 1 (qty 50 -> 30).
                               // So Ask ID 6 is still present with remaining qty 20.
                               // Bid ID 1 is gone now.
                               // Bid ID 2 (originally 99,100).
                               // The modify should make Bid ID 2 (100, 10) matching Ask ID 6 (100, 20).
                               // Ask ID 6 remaining should be 10. Bid ID 2 gone.
    
    // --- Final State ---
    std::cout << "\n--- Final Orderbook State ---" << std::endl;
    PrintOrderbook(orderbook);

    std::cout << "\nSimulation Complete." << std::endl;

    return 0;
}