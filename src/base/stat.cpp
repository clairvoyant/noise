#include "noise/def.h"
#include "noise/noise.h"
#include "noise/stat.h"
#include "noise/util.h"
#include "noise/broker.h"

namespace noise {

Stat::Stat(void)
{
    equity_series_ = nullptr;
}

Stat::~Stat(void)
{
    TRACE_LINE
}

/*
Start                     2004-08-19 00:00:00
End                       2013-03-01 00:00:00
Duration                   3116 days 00:00:00
Exposure Time [%]                       94.27
Equity Final [$]                     68935.12
Equity Peak [$]                      68991.22
Return [%]                             589.35
Buy & Hold Return [%]                  703.46
Return (Ann.) [%]                       25.42
Volatility (Ann.) [%]                   38.43
Sharpe Ratio                             0.66
Sortino Ratio                            1.30
Calmar Ratio                             0.77
Max. Drawdown [%]                      -33.08
Avg. Drawdown [%]                       -5.58
Max. Drawdown Duration      688 days 00:00:00
Avg. Drawdown Duration       41 days 00:00:00
# Trades                                   93
Win Rate [%]                            53.76
Best Trade [%]                          57.12
Worst Trade [%]                        -16.63
Avg. Trade [%]                           1.96
Max. Trade Duration         121 days 00:00:00
Avg. Trade Duration          32 days 00:00:00
Profit Factor                            2.13
Expectancy [%]                           6.91
SQN                                      1.78
_strategy              SmaCross(n1=10, n2=20)
_equity_curve                          Equ...
_trades                       Size  EntryB...
dtype: object
 */
void Stat::plot(void)
{
    TRACE_LINE

    uprint::print("Stat", m_stat);
}

void Stat::on_trade_day(void)
{
    if (equity_series_ == nullptr) {
        equity_series_ = data_->create_series(INDICATOR_EQUITY);
        equity_series_->figure = "Equity";
        equity_series_->name = "cash";
    }

    // cash + stock
    equity_series_->data.push_back(broker_->get_equity());
}

void Stat::on_finish(void)
{
    calc_stat();
    generate_trade_series();
    generate_equity_series();
}

void Stat::generate_trade_series(void)
{
    auto a_series = data_->create_series(INDICATOR_TRADE);
    auto ptrades = std::make_shared<struct tradeseries>();

    a_series->extra.ptrades = ptrades;
    a_series->figure = "Profit & Loss";
    a_series->name = "PNL";
    a_series->sign = "triangle";

    const auto closed_trades = broker_->get_trades();
    if (closed_trades.size() <= 0) {
        return;
    }

    for (auto iter = closed_trades.begin(); iter != closed_trades.end(); iter++) {
        ptrades->trades.push_back(*iter);
    }

    //
    const auto ptime = a_series->extra.ptime;
    //init zero
    a_series->data.resize(ptime->time.size());

#if 1
    //override pnl by date
    int index = 0;
    for (auto trade_iter = closed_trades.begin(); trade_iter != closed_trades.end(); trade_iter++) {
        time_t time = trade_iter->exit_time;
        float pnl = trade_iter->profit()*100;

        for (; index < ptime->time.size();) {
            if (ptime->time[index] == time) {
                LOGI("match time:{} {} pnl:{:6.2f}%", ptime->time[index], time, pnl);
                a_series->data[index] = pnl; //trade_iter->PNL;
                break;
            }
            index++;
        }
        ptrades->trades.push_back(*trade_iter);
    }
    //debug
    // int i = 0;
    // for (auto iter = a_series->data.begin(); iter != a_series->data.end(); iter++) {
    //     int date = utime::get_date(a_series->extra.ptime->time.at(i));
    //     LOGD("pnl chart pnl:{} date:{}", *iter, date);
    //     i++;
    // }
#endif
}

void Stat::generate_equity_series(void)
{

}

void Stat::calc_stat(void)
{
    TRACE_LINE

    const auto trades = broker_->get_trades();
    if (trades.size() <= 0) { return; }

    struct bar first_bar, last_bar;
    broker_->get_first_and_last_bars(first_bar,last_bar);

    float equity_base = 10000;
    float equity_cur  = equity_base;
    int win_cnt = 0;

    float best_trade_on_pnl,   worst_trade_on_pnl;
    float best_trade_on_ratio, worst_trade_on_ratio;
    float avg_profit_ratio = 0;
    std::vector<float> vec_pnl;
    std::vector<int> vec_duration;  //second
    std::vector<int> vec_exposure;
    int max_duration = 0;           //second

    const auto &trade0 = *trades.begin();
    best_trade_on_pnl = worst_trade_on_pnl = trade0.PNL;
    best_trade_on_ratio = worst_trade_on_ratio = trade0.profit();

    m_stat.start_time = first_bar.time;
    m_stat.end_time = last_bar.time;

    //printf("first_bar.time :%d\n", first_bar.time);
    //printf("last_bar.time  :%d\n", last_bar.time);

    //m_stat.duration = utime::get_duration(first_bar.time, last_bar.time);
    m_stat.duration = utime::get_duration(last_bar.time - first_bar.time);
    vec_exposure.resize(m_stat.duration);

    for (auto it = trades.begin(); it != trades.end(); ++it) {
        const struct trade& trade = *it;

        // if (m_stat.start_time > trade.entry_time) {
        //     m_stat.start_time = trade.entry_time;
        // }
        // if (m_stat.end_time < trade.exit_time) {
        //     m_stat.end_time = trade.exit_time;
        // }
        m_stat.PNL += trade.PNL;
        equity_cur += trade.PNL;

        if (m_stat.equity_peak < equity_cur) {
            m_stat.equity_peak = equity_cur;
        }
        if (trade.PNL > 0) {
            win_cnt++;
        }
        if (best_trade_on_pnl < trade.PNL) {
            best_trade_on_pnl = trade.PNL;
        }
        if (worst_trade_on_pnl > trade.PNL) {
            worst_trade_on_pnl = trade.PNL;
        }

        float profit = trade.profit();
        if (best_trade_on_ratio < profit) {
            best_trade_on_ratio = profit;
        }
        if (worst_trade_on_ratio > profit) {
            worst_trade_on_ratio = profit;
        }
        avg_profit_ratio += profit;

        int dura = (int)(trade.exit_time - trade.entry_time);
        if (max_duration < dura) {
            max_duration = dura;
        }

        vec_pnl.push_back(trade.PNL);
        vec_duration.push_back(dura);

#if 0
        int dura_offset = trade.entry_time - first_bar.time;
        int dura_days = trade.exit_time - trade.entry_time;
        for (int i = 0; i <= dura_days; i++) {
            if (dura_offset+i >= vec_exposure.size()) {
                printf("[err] dura_offset+i:%d %d\n", dura_offset, i);
                break;
            }
            vec_exposure.at(dura_offset+i) = 1;
        }
#endif
    }
    int exposion_days = umath::sum(vec_exposure);
    m_stat.exposure_time = (float) exposion_days / (float) m_stat.duration;

    //
    m_stat.trade_cnt = (int) trades.size();
    m_stat.equity_final = equity_base+m_stat.PNL;
    m_stat.return_final = m_stat.PNL / equity_base;
    m_stat.return_buy_and_hold = (last_bar.close - first_bar.open) / first_bar.open;
    m_stat.win_ratio = (float) win_cnt / (float)trades.size();
    m_stat.best_trade_on_PNL = best_trade_on_pnl;
    m_stat.worst_trade_on_PNL = worst_trade_on_pnl;
    m_stat.best_trade = best_trade_on_ratio;
    m_stat.worst_trade = worst_trade_on_ratio;
    m_stat.avg_trade = avg_profit_ratio / trades.size();

    //convert second to duration
    m_stat.avg_trade_duration = (float) umath::mean(vec_duration);
    m_stat.max_trade_duration = (float) max_duration;

    m_stat.avg_trade_duration = (float) utime::get_duration((time_t)m_stat.avg_trade_duration);
    m_stat.max_trade_duration = (float) utime::get_duration((time_t)m_stat.max_trade_duration);

    //
    m_stat.SQN = sqrtf((float)trades.size()) * umath::mean(vec_pnl) / umath::stddev(vec_pnl);
    //s.loc['SQN'] = np.sqrt(n_trades) * pl.mean() / (pl.std() or np.nan)


    //update PNL series
}

void Stat::init(std::shared_ptr<Broker> broker, std::shared_ptr<Data> data)
{
    broker_ = broker;
    data_ = data;
    memset(&m_stat, 0, sizeof(m_stat));
}


}