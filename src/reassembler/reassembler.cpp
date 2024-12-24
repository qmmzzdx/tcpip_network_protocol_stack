#include "reassembler.h"

// 插入数据片段到重组器中
void Reassembler::insert(uint64_t first_index, std::string data, bool is_last_substring)
{
    // 如果输出流已经关闭，或者插入位置超出EOF或可用容量，则直接返回
    if (output_.writer().is_closed() || first_index >= eof_index_ || first_index >= first_unassembled_index_ + output_.writer().available_capacity())
    {
        std::cerr << "Reassembler::insert error.\n";
        return;
    }

    // 如果这是最后一个子串，更新EOF索引
    if (is_last_substring)
    {
        eof_index_ = first_index + static_cast<uint64_t>(data.size());
    }

    // 计算有效插入的开始和结束索引
    uint64_t beg_idx = std::max(first_index, first_unassembled_index_);
    uint64_t end_idx = std::min(first_index + static_cast<uint64_t>(data.size()), first_unassembled_index_ + output_.writer().available_capacity());

    // 如果结束索引小于等于开始索引，检查是否需要关闭输出流
    if (end_idx <= beg_idx)
    {
        if (first_unassembled_index_ >= eof_index_)
        {
            output_.writer().close();
        }
        return;
    }

    // 创建一个新的数据区间
    Interval itv(beg_idx, end_idx, data.substr(beg_idx - first_index, end_idx - beg_idx));

    // 如果缓冲区为空，直接插入
    if (buffers_.empty())
    {
        buffers_.push_back(itv);
    }
    else
    {
        // 找到插入位置
        auto it = std::lower_bound(buffers_.begin(), buffers_.end(), itv);
        auto prev = it;

        // 如果是第一个位置，直接插入
        if (it == buffers_.begin())
        {
            buffers_.insert(it, itv);
            prev = buffers_.begin();
        }
        else
        {
            // 否则检查前一个区间是否可以合并
            prev = std::prev(it);
            if (prev->end_idx >= itv.beg_idx)
            {
                if (prev->end_idx <= itv.end_idx)
                {
                    prev->interval_str = prev->interval_str.substr(0, itv.beg_idx - prev->beg_idx) + itv.interval_str;
                    prev->end_idx = itv.end_idx;
                }
                itv = *prev;
            }
            else
            {
                buffers_.insert(it, itv);
                prev++;
            }
        }

        // 合并重叠的区间
        while (it != buffers_.end() && itv.end_idx >= it->beg_idx)
        {
            if (it->end_idx > itv.end_idx)
            {
                prev->interval_str.replace(it->beg_idx - itv.beg_idx, std::string::npos, it->interval_str);
                prev->end_idx = it->end_idx;
            }
            it = buffers_.erase(it);
            itv = *prev;
        }
    }

    // 将可以输出的数据推送到输出流
    std::erase_if(buffers_, [this](const Interval& interval)
    {
        if (interval.beg_idx == first_unassembled_index_)
        {
            output_.writer().push(interval.interval_str);
            first_unassembled_index_ = interval.end_idx;
            return true;
        }
        return false;
    });

    // 如果所有数据都已重组，关闭输出流
    if (first_unassembled_index_ >= eof_index_)
    {
        output_.writer().close();
    }
}

// 返回当前缓冲区中待重组的字节数
uint64_t Reassembler::bytes_pending() const
{
    return std::accumulate(buffers_.begin(), buffers_.end(), uint64_t{0}, [](uint64_t sum, const Interval& interval)
    {
        return sum + (interval.end_idx - interval.beg_idx);
    });
}
