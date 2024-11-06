#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return close_;
}

void Writer::push( string data )
{
  // Your code here.
  if ( close_ )
    return;

  uint64_t len = std::min( available_capacity(), data.size() );
  buffer_.append( data.substr( 0, len ) );
  push_number_ += len;
}

void Writer::close()
{
  // Your code here.
  close_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - ( push_number_ - pop_number_ );
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return push_number_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return close_ && buffer_.empty();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return pop_number_;
}

string_view Reader::peek() const
{
  // Your code here.
  return buffer_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t pop_len = std::min( len, buffer_.size() );
  buffer_.erase( 0, pop_len );
  pop_number_ += pop_len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffer_.size();
}
