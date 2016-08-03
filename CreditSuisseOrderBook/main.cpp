//
//  main.cpp
//  CreditSuisseOrderBook
//
//  Created by Dan Akselrod on 8/2/16.
//  Copyright (c) 2016 dantap. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_map>
#include <iomanip>


using namespace std;

typedef string SymbolType;
    // TODO: make a class to hold symbol that wouldn't need to malloc
    // i.e. a c++ class to wrap around {{ char symbol_[MAX_SYMBOL] }}
typedef float  PriceType;
typedef long IDType;
typedef long SizeType;
typedef char SideType;

struct OrderType {
    SideType side;
    PriceType price;
    SizeType size;
};

// TODO: more descriptive error

class Error{}; //  error with input
class InternalError{}; // Internal error


////////////////////////////// ORDER BOOK CODE ////////////


class HalfOrderBook
{
public:
    void add(PriceType price, SizeType size)
    {
        price_levels_[price] += size;
    }
    
    void del(PriceType price, SizeType size)
    {
        auto pl = price_levels_.find(price);
        SizeType new_size = (pl->second -= size);
        if (new_size <= 0)
        {
            price_levels_.erase(pl);

            if (new_size < 0)
                throw InternalError();
        }
    }
    
    void printUp(ostream &os)
    {
        for (auto i = price_levels_.begin(); i != price_levels_.end(); ++i)
        {
            os << setw(10) << right << i->second << ' ' << left << i->first << endl;
        }
    }
    
    void printDown(ostream &os)
    {
        for (auto i = price_levels_.rbegin(); i != price_levels_.rend(); ++i)
        {
            os << setw(10) << right << i->second << ' ' << left << i->first << endl;
        }
    }

private:
    // aggregates per price
    
    // TODO: a more robust structure then a map would be a
    // shifted array, where price was in smallest units (cents, 10th of cents)
    // with an offset. For example if we expect a price of a stock to be between $10..20
    // and penny increments, the array would have 1000 elements, with [0] at $10.00,
    // [1] at $10.01 etc until [1000] at $19.99.
    map<PriceType, SizeType> price_levels_;
};

class OrderBook
{
public:
    void add(IDType id, const OrderType &order)
    {
        switch (order.side)
        {
            case 'B':
                buy_.add(order.price, order.size);
                break;
            case 'S':
                sell_.add(order.price, order.size);
                break;
            default:
                throw Error();
        }
        auto o = orders_.find(id);
        if (o != orders_.end())
        {
            cerr << "Duplicate order entry " << id << endl;
        }
        orders_[id] = order;
    }
    
    void del(IDType id)
    {
        auto o = orders_.find(id);
        if (o == orders_.end())
        {
            cerr << "Non existent order delete : " << id << endl;
            return;
        }
        OrderType ob = o->second; // yes a copy
        orders_.erase(o); // delete now, in case we have an exception
        switch (ob.side) {
            case 'B':
                buy_.del(ob.price, ob.size);
                break;
            case 'S':
                sell_.del(ob.price, ob.size);
                break;
            default:
                throw InternalError();
                break;
        }
        
    }
    
    void print(ostream &os)
    {
        buy_.printUp(os);
        os << "  //BUY\\\\  ===  \\\\SELL///\n";
        sell_.printDown(os);
    }
    
private:
    
    unordered_map<IDType, OrderType> orders_;
    
    HalfOrderBook buy_;
    HalfOrderBook sell_;
};

class PipeParser
{
public:
    
    PipeParser(const string &line) : line_(line), old_pipe_(0), pipe_(0) {}
    

    void readOne(string &s)
    {
        advance();
        s.assign(line_, old_pipe_, pipe_);
    }
    
    void readOne(long &l)
    {
        advance();
        char *err = NULL;
        l = strtol(&line_[old_pipe_], &err, 10);
        if (err[0] != '\0' && err != &line_[pipe_])
            throw Error();
    }
    
    void readOne(float &f)
    {
        advance();
        char *err = NULL;
        f = strtof(&line_[old_pipe_], &err);
        if (err[0] != '\0' && err != &line_[pipe_])
            throw Error();
    }
    
    void readOne(char &c)
    {
        advance();
        if (pipe_ - old_pipe_ != 1)
            throw Error();
        
        c = line_[old_pipe_];
    }
    
private:
    void advance()
    {
        if (pipe_ == string::npos)
            throw Error();
        old_pipe_ = pipe_;
        if (old_pipe_ != 0)
            old_pipe_ ++;
        pipe_ = line_.find('|', old_pipe_);
    }

    string::size_type pipe_;
    string::size_type old_pipe_;
    const string &line_;
};

////////////////////////////// OPERATIONS /////////////////
int main(int argc, const char * argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: ob_test <input_file>\n";
        return 1;
    }
    
    unordered_map<SymbolType, OrderBook> Books;
    
    ifstream file(argv[1]);
    if (file.bad())
    {
        cerr << "Can't open " << argv[1] << " for input\n";
        return 1;
    }
    string line;
    while (getline(file, line))
        // TODO: slow since it's a malloc, but IRL we wouldnt be reading a text file
    {
        try {
            PipeParser pp(line);
            
            SymbolType sym;
            pp.readOne(sym);
            
            char cmd;
            pp.readOne(cmd);
            switch (cmd)
            {
                case 'A':
                { // add
                    IDType id;
                    OrderType order;
                    pp.readOne(order.side);
                    pp.readOne(id);
                    pp.readOne(order.size);
                    pp.readOne(order.price);
                    
                    Books[sym].add(id, order);
                    break;
                }
                    
                case 'D':
                { // delete
                    IDType id;
                    pp.readOne(id);
                    auto i = Books.find(sym);
                    if (i == Books.end())
                        throw Error();
                    
                    i->second.del(id);
                    break;
                }
                    
                default:
                    throw Error();
                    
            }
        }
        catch(Error &e)
        {
            cerr << "Error processing: " << line << endl;
        }
        catch(InternalError &be)
        {
            cerr << "Internal Error processing: " << line << endl;
        }
    }
    
    for (auto b = Books.begin(); b != Books.end(); ++b)
    {
        cout << "\n[[[ " << b->first << " ]]]\n";
        b->second.print(cout);
    }
    
    return 0;
}
