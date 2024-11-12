**converting 解决思路**
首先要知道的信息:c++的uint数据是*循环*的,即当超过了最大值以后,会自动从0开始
1. abs -> seq: seq = abs + ISN
2. seq -> abs: abs = seq - ISN
    - 第一直觉是将seq - ISN,得到的就是abs,但是这里存在一个问题,因为我们现在是在32位的视角看64位,例如32位看17,2^32 + 17也等于17,而64位看的时候就是2^32 + 17
    - check_point:它是当前连续流的最新索引(注意 会存在非连续流存在reassembler里面),且我们该兴趣的只有他的高32位

**receiver解决思路**
注意:`ack_number`是返回下一个想要的连续的数据index
1. 进行握手初始化(SYN信号)
2. 将数据insert到reassembler中,并更新ack_number
3. 若该数据流被读取完毕(write.close()),则ack_number + 1 (这里+1表示告诉sender,我已经读取完所有数据,且读取了final信号);