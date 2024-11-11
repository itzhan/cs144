#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();
  // 表示右边界
  uint64_t right_bound = writer.bytes_pushed() + writer.available_capacity();

  // 如果是last数据,进行记录
  if ( is_last_substring ) {
    eof = true;
    end_index = first_index + data.size();
  }

  // 如果该数据的左边界大于buffer,直接结束
  if ( first_index >= right_bound )
    return;

  // 截断data,防止出现超过buffer的数据
  if ( ( first_index + data.size() ) > right_bound ) {
    data = data.substr( 0, right_bound - first_index );
  }

  // case1: 当index小于当前流索引,且出现overlap的情况
  if ( first_index < next_index && ( first_index + data.size() ) > next_index ) {
    data = data.substr( next_index - first_index );
    first_index = next_index;
  }

  // case2: 当index > 当前流索引,则存入map,也需要处理overlap情况
  if ( first_index > next_index ) {
    auto result = tem_buffer.insert({first_index, data});
    store_bytes += data.size();
     // 若已经存在,且值相同,则直接结束
    if (!result.second && (data.size() <= tem_buffer[first_index].size())){
      store_bytes -= data.size();
      return;
    }
     // 若已经存在,则取最大值
    else if (!result.second && (data.size() > tem_buffer[first_index].size())){
      store_bytes -= data.size();
      store_bytes += data.size() - tem_buffer[first_index].size();
      tem_buffer[first_index] = data;
    }
    // 插入成功,从头开始合并
    auto pre = tem_buffer.begin();
    auto current = ++tem_buffer.begin();
    while (current != tem_buffer.end()){
      uint64_t pre_L = pre->first;
      uint64_t pre_R = pre->first + pre->second.size();
      uint64_t cur_L = current->first;
      uint64_t cur_R = current->first + current->second.size();

      // 完全被overlap
      if ((cur_L >= pre_L) && (cur_R <= pre_R)){
        store_bytes -= current->second.size();
        current = tem_buffer.erase(current);
      }
      // 合并
      else if ((cur_L >= pre_L) && (cur_L <= pre_R) && (cur_R > pre_R)){
        uint64_t overlap = pre_R - cur_L;
        store_bytes -= overlap;
        pre->second += current->second.substr(overlap);
        current = tem_buffer.erase(current);
      }
      else {
        ++pre;
        ++current;
      }
    }
  }

  // case3: 当index = 当前索引,则推进Stream,并update当前map
  if ( first_index == next_index ) {
    // 直接推进即可
    writer.push( data );
    next_index += data.size();

    // update map
    auto it = tem_buffer.begin();
    while ( it != tem_buffer.end() ) {
      uint64_t item_left = it->first;
      uint64_t item_right = it->first + it->second.size();
      auto item_data = it->second;
      auto item_size = it->second.size();

      // 右边界小于当前索引 --> 被完全overlop ---> 则直接drop
      if ( item_right <= next_index ) {
        store_bytes -= item_size;
        it = tem_buffer.erase( it );
      }

      // 左边界 < 当前索引, 右边界 > 当前索引 --> 右多余的overlap,进行切割并插入
      else if ( item_left < next_index && item_right > next_index ) {
        item_data = item_data.substr( next_index - item_left );
        item_left = next_index;
        // 保证右边界没有超过right_bound
        if ( item_right > right_bound ) {
          item_data = item_data.substr( 0, right_bound - item_left );
        }
        // 插入数据
        writer.push( item_data );
        store_bytes -= item_size;
        next_index += item_data.size();
        it = tem_buffer.erase( it );
      }

      // 相等的情况
      else if ( item_left == next_index ) {
        if ( item_right > right_bound ) {
          item_data = item_data.substr( 0, right_bound - item_left );
        }
        writer.push( item_data );
        store_bytes -= item_size;
        next_index += item_data.size();
        it = tem_buffer.erase( it );
      }

      else {
        ++it;
      }
    }
  }

  // case4:到达终点
  if ( eof && next_index >= end_index ) {
    writer.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return store_bytes;
}