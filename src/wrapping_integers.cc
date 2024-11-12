#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32( n + zero_point.raw_value_ );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t offset = this->raw_value_ - zero_point.raw_value_;
  // Calculate the high 32 bits of the checkpoint
  uint64_t base_index = checkpoint & 0xFFFFFFFF00000000;
  uint64_t candidate1 = base_index + offset;
  uint64_t candidate2 = candidate1 + 0x1'0000'0000;
  uint64_t candidate3 = ( candidate1 >= 0x1'0000'0000 ) ? candidate1 - 0x1'0000'0000 : candidate1;

  // Find the candidate closest to the checkpoint
  uint64_t result = candidate1;
  if ( abs_diff( candidate2, checkpoint ) < abs_diff( result, checkpoint ) ) {
    result = candidate2;
  }
  if ( candidate1 >= 0x1'0000'0000 && abs_diff( candidate3, checkpoint ) < abs_diff( result, checkpoint ) ) {
    result = candidate3;
  }
  return result;
}
