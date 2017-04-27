/*
                       大药房发药及输送系统主控板程序

  2016.02.14 将网络主板升级为装机用程序，除去不必要的程序段
  在以前主控板程序的基础上进行升级
  2016.02.15 增加药品输送及其他辅助控制程序段
  网络通讯也已经重新测试完毕
  2016.02.24 CAN2，CAN3出现故障，但原先的老版本程序，能运行。现已更换成老版本。
  2016.06.11 在侯工调试的基础上，进行整合大药房发药系统的程序
             伺服启动标志 Servo_Begin_Sign ，电磁铁动作完毕后，延时5秒（默认）此标志置高。
  2016.06.18 与小梁进行通讯对接,发药已正常。
  2016.06.19 对程序的的层数进行了整理，并完善了底层板BITE部分
  2016.06.21 发药若地址不对，不对底层板发送数据。
             发药动作完毕后，延时1秒时间，触发伺服动作。

  2016.06.22 已与小梁进行了数据通讯，0XC0仅由上位机请求时再发，否则不上发。
             电磁铁动作后，延时1秒，传送带开始动作已实现。

  2016.06.25 根据现场需求，电磁铁动作完毕后，延时的时间去除。
  2016.07.19  为了底层机构顺利初始化，在main()函数开始的位置，将delay_ms(4000);延时改为delay_ms(15000);

  2017.03.02  当底层某个发药驱动板出现通讯故障时，会导致系统暂停。现拟采用定长强制中断
              定时的时长默认为5000ms， 点开ZK.h文件中修改这个宏定义的值即可（ Bite_Delay_Time  500  ）
			  同时发送的结果数据由以前的0XAA，改为0X55

*/
//-----------------------------------------------------------------------------
// 包含文件
//-----------------------------------------------------------------------------
#include <zk.h>
#include <math.h>

//-----------------------------------------------------------------------------
// C8051F040的SFR定义
//-----------------------------------------------------------------------------
sfr16 ADC0 = 0xbe;  // ADC0 data

//-----------------------------------zk.c--------------------------------------
// 全局变量
//-----------------------------------------------------------------------------
//定义定时器的软件计数器
unsigned char  T0Counter1 = 0; //运行指示灯
unsigned char  T0Counter2 = 0; //CAN 发送超时计数器
unsigned int   T0Counter3 = 0; //BITE反馈给上位机
unsigned char  T0Counter4 = 0; //对OK信息的处理和上报

xdata unsigned char  T0Counter12 = 0; //串口控制器使用1
xdata unsigned char  T0Counter13 = 0; //串口控制器使用2
xdata unsigned int   T0Counter14 = 0; //电磁铁发药结束后，延时用
xdata unsigned int   T0Counter15 = 0; //电磁铁发药结束后，延时用

unsigned char Group_index_DCT = 0;    //电磁铁动作批次号也即数据序列
unsigned char Board_Address_Number = 0; //自检用

unsigned char ALL_Action_OK_Sign = 0;
unsigned char ALL_Action_OK_Sign_last = 0;
unsigned char ALL_Result_OK_Sign = 0;

unsigned char OK_Sign_TX_Amount = 0; //0k信号的发送次数
unsigned char temppage = 0;

unsigned char Zijian_Sign = 0xaa;     //自检标志，0xaa退出自检模式，0X55进入自检模式

xdata SYSDAT  SysData;
xdata  unsigned char  databuf[8];

xdata unsigned char LED_Address _at_ 0xFB00 ;
xdata unsigned char LED_BUF = 0;

bit Command_Finish_Sign = NO;   //指令结束和开始标志
bit Command_Begin_Sign = NO;    //传送带启动指令
bit Servo_Begin_Sign = NO;      //伺服触发标志
bit  Answer_ok = NO;
unsigned char Board_ID = 0x00;

unsigned int  Proce_Count = 0x00;          //自动操作，动作到位后的等待时间
unsigned char Procession1 = 0x00;          //自动操作步骤代码
unsigned char Run_modeFayaoL = 0;
unsigned char Run_modeFayaoR = 0;
unsigned char Run_mode = 0;
unsigned char SNSORSTATUS1 = 0;
unsigned char SNSORSTATUS2 = 0;
unsigned char RUNSTATUS1 = 0;
unsigned char RUNSTATUS2 = 0;

unsigned char MAINRUNSTATUSL = 0;
unsigned char MAINRUNSTATUSL1 = 0;

unsigned char Runbiaoji1 = 0;
unsigned char Runbiaoji2 = 0;
bit Tansflag = 0;

unsigned char Runbiaoji3 = 0;
unsigned char Runbiaoji4 = 0;
unsigned char FAYAOinput_pra[4];
unsigned char communication_step = 0;
bit  ONCEFLAG = NO;
bit  Alarm_stepflagL = NO; //故障点标志位
bit  Alarm_stepflagR = NO; //故障点标志位
bit  ALARM_ONCEFLAGL = NO;
bit  ALARM_ONCEFLAGR = NO;
bit  Servo_left_Begin_Sign = NO;
bit  Servo_right_Begin_Sign = NO;
bit  FaYao_Prosetion_Sign = NO;
bit  Active_Data_Begin_Sign_last = NO;
bit  Active_Data_Begin_Sign = NO;

xdata Servomotordisplace Servodisplace;
xdata  float m_floatServodisplace = 0.0;
xdata  unsigned int m_intServodisplace = 0;
xdata  unsigned char Run_REAFY1 = 0;
xdata  unsigned char Run_REAFY2 = 0;
xdata  unsigned char Run_ALARM1 = 1;
xdata  unsigned char Run_ALARM2 = 1;
xdata  unsigned char Mode_ALARM1 = 0;
xdata  unsigned char Mode_ALARM2 = 0;
xdata  unsigned char Mode_ALARM3 = 0;
xdata  unsigned char Mode_ALARM4 = 0;
xdata  unsigned char Mode_adress1 = 0;
xdata  unsigned char Mode_adress2 = 0;
xdata  unsigned char Mode_adress3 = 0;
xdata  unsigned char Mode_adress4 = 0;
xdata  unsigned char Basket_stat = 0;
xdata  unsigned char strat_flag = 0;
xdata  unsigned char CAN_index = 0;
xdata  unsigned int  AUTO_countimerL = 0;
xdata  unsigned int  AUTO_countimerR = 0;

//xdata unsigned char TEST_ADDRESS1=0;
//xdata unsigned char TEST_ADDRESS2=0;
//xdata unsigned char TEST_COMMAND=0;
//////////////////////////////////////////////////////////////////////////////////
// 读取底层驱动板输入信号的状态
// 电磁铁驱动板编号 number ：0～Driver_Board_Amount-1
//////////////////////////////////////////////////////////////////////////////////
bit Get_DCT_Input_State(unsigned char  number)
{
    return (bit)( SysData.DCT_Input_Command[number / 8] & (0x01 << (number % 8)) ) ;
}
//-----------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////
// 设置电磁铁驱动板输入的状态，也即输入指令的状态标志，为1时，表示此命令有效，为0时，表示此命令无效
// 电磁铁驱动板编号端口号 number ：0～Driver_Board_Amount-1 ;
// 状态   设置为1； 设置为0；
//////////////////////////////////////////////////////////////////////////////////
void Set_DCT_Input_State(unsigned char  number, bit state)
{
    if(state == 1)
        SysData.DCT_Input_Command[number / 8] |= (0x01 << (number % 8));
    else
        SysData.DCT_Input_Command[number / 8] &= ~(0x01 << (number % 8));
}

//-----------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////
// 设置电磁铁驱动板动作次数OK的状态，为1表示此槽位该批次动作次数OK.
// 电磁铁驱动板编号端口号 number ：0～Driver_Board_Amount-1 ;
// 状态   设置为1； 设置为0；
//////////////////////////////////////////////////////////////////////////////////
void Set_DCT_OK_Result(unsigned char  number, bit state)
{
    if(state == 1)
        SysData.DCT_OK_Result[number / 8] |= (0x01 << (number % 8));
    else
        SysData.DCT_OK_Result[number / 8] &= ~(0x01 << (number % 8));
}

//-----------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////////
// 设置电磁铁动作结束的状态，为1表示此槽位该批次动作结束
// 端口号 number ：0～Driver_Board_Amount-1 ;
// 状态   设置为1； 设置为0；
//////////////////////////////////////////////////////////////////////////////////
void Set_DCT_OK_Action(unsigned char  number, bit state)
{
    if(state == 1)
        SysData.DCT_OK_Action[number / 8] |= (0x01 << (number % 8));
    else
        SysData.DCT_OK_Action[number / 8] &= ~(0x01 << (number % 8));
}

//////////////////////////////
//定时0中断,模式1,16位定时计数器, 时钟12分频 ,高优先级
//T0=65536-1000us*11.0592/4=0xF533
//////////////////////////////
void TIME0_ISR (void) interrupt 1
{
    unsigned char i ;
    unsigned char tempsfr ;
    tempsfr = SFRPAGE ;
    SFRPAGE = TIMER01_PAGE;
    // TH0=0x94;    //Reset 10ms interrupt  //时钟采用4分频
    // TL0=0x00 ;
    // TH0=0xF5;    //Reset 1ms interrupt //时钟采用4分频
    // TL0=0x33 ;
    TH0 = 0xDC;  //Reset 10ms interrupt  //时钟采用12分频
    TL0 = 0x00 ;

    WDTCN = 0xA5;  //看门狗复位

    T0Counter1++;
    T0Counter2++;
    T0Counter3++;
    T0Counter4++;
    T0Counter12++;
    T0Counter13++;
    T0Counter14++;    //电磁铁发药结束后，开始计时
    T0Counter15++;    //电磁铁发药结束后，开始计时

    for(i = 0; i < Driver_Board_Amount; i++)
    {
        if( SysData.DCT_Bite[i] < 200 ) SysData.DCT_Bite[i]++;
    }

    if( ALL_Action_OK_Sign != ALL_Action_OK_Sign_last )
    {
        if(ALL_Action_OK_Sign == 0xBB)  T0Counter14 = 0;
    }
    ALL_Action_OK_Sign_last = ALL_Action_OK_Sign;

    if( Active_Data_Begin_Sign != Active_Data_Begin_Sign_last )
    {
        if(Active_Data_Begin_Sign == YES)
        {
            T0Counter15 = 0;
            FaYao_Prosetion_Sign = YES;
        }
    }
    Active_Data_Begin_Sign_last = Active_Data_Begin_Sign;

    LED_Address = LED_BUF;     //LED输出控制
    SFRPAGE = tempsfr ;
}

//--------------------------------------------------------------
void CAN_AUTO_RESET()
{
//    if(CAN1FaultCounter > 10)
//    {
//        init_can1() ;
//        CAN1FaultCounter = 0 ;
//    }

    if(CAN2FaultCounter > 10)
    {
        init_can2() ;
        CAN2FaultCounter = 0;
    }

    if(CAN3FaultCounter > 10)
    {
        init_can3() ;
        CAN3FaultCounter = 0;
    }
}

//-------------------------------------------
//当底层驱动板发药的次数不相符时，主控板就能接收到底层驱动板发的该槽位的反馈数据，
//且及时将此信息反馈给上位机
//其中第5个字节就是该槽位的故障代码
//-------------------------------------------
void can_rx_from_DCT_amount_feedback(unsigned char *buf)
{
    // unsigned char i,sum;
    TransBuf1.amount_buf.command = buf[0];
    TransBuf1.amount_buf.index1 =  buf[1];
    TransBuf1.amount_buf.address1 = buf[2];
    TransBuf1.amount_buf.address2 = buf[3];
    TransBuf1.amount_buf.bite =  buf[4];
    TransBuf1.amount_buf.blank1 =  buf[5];
    TransBuf1.amount_buf.amount =  buf[6];
    Uart1Send();
}
//----------------------------------------------------------------------------
//接受底层驱动板送过来的BITE信息及动作OK信息
//----------------------------------------------------------------------------
//都按照标准的一个机架16排计算，若有的机架，不够16排，通过上位机在界面上调整。
//-----------------------------------------------------------------------------
void can_rx_from_DCT_bite_feedback(unsigned char *buf)
{
    unsigned char Number_amout_1;

    //接收到动作OK指令
    Number_amout_1 = (buf[2] & 0x0F)
                     + ((((buf[2] & 0xE0) >> 5) & 0x07) - 1) * Floor_Number;

    if( Command_Finish_Sign == YES )                 //只有在指令结束后，才判断
    {
        if( ( buf[5] == 0xAA ) && ( Get_DCT_Input_State(Number_amout_1) == YES ) ) //此驱动板的确接受到过控制输入指令
        {
            Set_DCT_OK_Result(Number_amout_1, OK);       //表示此排都已经动作结束并次数OK
        }

        if( (buf[6] == 0xBB) && ( Get_DCT_Input_State(Number_amout_1) == YES ) )  //此驱动板的确接受到过控制输入指令
        {
            Set_DCT_OK_Action(Number_amout_1, OK);        //表示此排都已经动作结束
        }
    }
    SysData.DCT_Bite[Number_amout_1] = 0;
}

//---------------------- 处理从上位机发过来的数据指令--------------------
void can_rx_from_SWJ_Zijian_Command(void)    //自检指令
{
    unsigned char i, sum;

    //------------------向上位机回馈-------------------------
    TransBuf1.amount_buf.command = 0x14;
    TransBuf1.amount_buf.address1 = 0xbf;
    TransBuf1.amount_buf.address2 = RecBuf1.act_buf.address2;
    TransBuf1.amount_buf.bite = 0x00;
    TransBuf1.amount_buf.blank1 = 0x00;
    TransBuf1.amount_buf.amount = 0x00;
    Uart1Send();                                 //命令回馈

    if(   (RecBuf1.act_buf.address1 == 0xbf)     //接受到自检指令
            && (RecBuf1.act_buf.address2 == 0x55)
      )
    {
        Zijian_Sign = 0x55;
    }
    else if(   (RecBuf1.act_buf.address1 == 0xbf) //接受结束自检指令
               && (RecBuf1.act_buf.address2 == 0xaa)
           )
    {
        Zijian_Sign = 0xaa;
    }
    else
        Zijian_Sign = 0xaa;

    //----------------向底层驱动板发送自检指令---------------------------------
    CAN2TXbuffer1.act_buf.command = RecBuf1.act_buf.command;
    CAN2TXbuffer1.act_buf.index1 = RecBuf1.act_buf.index1;
    CAN2TXbuffer1.act_buf.address1 = RecBuf1.act_buf.address1;
    CAN2TXbuffer1.act_buf.address2 = RecBuf1.act_buf.address2;
    CAN2TXbuffer1.act_buf.off_time = 0x00;
    CAN2TXbuffer1.act_buf.on_time = 0x00;
    CAN2TXbuffer1.act_buf.aim_amount = 0x00;

    sum = 0;
    i = 0;
    do
    {
        sum += CAN2TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN2TXbuffer1.act_buf.checkout = sum;
    can2_transmit(CAN2TXID_Uion, CAN2TXbuffer1.buf);
}

//---------------------- 处理从上位机发过来的数据指令--------------------
void can_rx_from_SWJ_Action_Command(void)
{
    unsigned char i, sum, Number_amout_2;

    if( (RecBuf1.act_buf.address1 == 0xbe)
            && (RecBuf1.act_buf.address2 == 0x66)
      )                         //向上位机发送始发包回馈信号
    {
        init_index_para();        //相关参数初始化
        Command_Begin_Sign = YES;
        Command_Finish_Sign = NO;

        //------------------向上位机回馈-------------------------
        Group_index_DCT = RecBuf1.act_buf.index1;
        TransBuf1.amount_buf.command = 0x80;
        TransBuf1.amount_buf.index1 = RecBuf1.act_buf.index1;
        TransBuf1.amount_buf.address1 = 0xaf;
        TransBuf1.amount_buf.address2 = 0x55;
        TransBuf1.amount_buf.bite = 0x00;
        TransBuf1.amount_buf.blank1 = 0x00;
        TransBuf1.amount_buf.amount = 0x00;
        Uart1Send();
        //----------------向发药底层驱动板发送准备好指令---------------------------------
        CAN2TXbuffer1.act_buf.command = RecBuf1.act_buf.command;
        CAN2TXbuffer1.act_buf.index1 = RecBuf1.act_buf.index1;
        CAN2TXbuffer1.act_buf.address1 = 0xbe;
        CAN2TXbuffer1.act_buf.address2 = 0x66;
        CAN2TXbuffer1.act_buf.off_time = 0x00;
        CAN2TXbuffer1.act_buf.on_time = 0x00;
        CAN2TXbuffer1.act_buf.aim_amount = 0x00;

        sum = 0;
        i = 0;
        do
        {
            sum += CAN2TXbuffer1.buf[i];
            i++;
        } while(i < 7);
        CAN2TXbuffer1.act_buf.checkout = sum;
        can2_transmit(CAN2TXID_Uion, CAN2TXbuffer1.buf);
        //------------------始发包中机械控制用----------------------
        //  Lan_backinfor(0x01,0x55);
        //	communication_step=1;

        //added begin
        //在收到发药起始包时启动地面输送线无限长距离、慢速运行
        ConveyingLongSlow_Start(0x01);//left run
        //added end

    }
    else if( (RecBuf1.act_buf.address1 == 0xbe) && (RecBuf1.act_buf.address2 == 0x88)	)
    {
        Command_Finish_Sign = YES;        //设立指令结束标志
        // ----------------向上位机结束包发送回馈信号----------------------
        Group_index_DCT = RecBuf1.act_buf.index1;
        TransBuf1.amount_buf.command = 0x80;
        TransBuf1.amount_buf.index1 = RecBuf1.act_buf.index1;
        TransBuf1.amount_buf.address1 = 0xaf;
        TransBuf1.amount_buf.address2 = 0x44;
        TransBuf1.amount_buf.bite = 0x00;
        TransBuf1.amount_buf.blank1 = 0x00;
        TransBuf1.amount_buf.amount = 0x00;
        Uart1Send();
        //---------------向底层驱动板发送指令结束命令----------------------
        CAN2TXbuffer1.act_buf.command = RecBuf1.act_buf.command;
        CAN2TXbuffer1.act_buf.index1 = RecBuf1.act_buf.index1;
        CAN2TXbuffer1.act_buf.address1 = 0xbe;
        CAN2TXbuffer1.act_buf.address2 = 0x88;
        CAN2TXbuffer1.act_buf.off_time = 0x00;
        CAN2TXbuffer1.act_buf.on_time = 0x00;
        CAN2TXbuffer1.act_buf.aim_amount = 0x00;

        sum = 0;
        i = 0;
        do
        {
            sum += CAN2TXbuffer1.buf[i];
            i++;
        } while(i < 7);
        CAN2TXbuffer1.act_buf.checkout = sum;
        can2_transmit(CAN2TXID_Uion, CAN2TXbuffer1.buf);

        //added begin
        //在收到发药结束包时停止地面输送线无限长距离、慢速运行
        ConveyingLongSlow_Stop();//地面输送线慢速模式结束
        //added end

        //---------结束包中机械动作部分-----------------------------------------
        switch(Run_mode)
        {
        case 1:
            Run_modeFayaoL = 1; //左发药
            break;
        case 2:
            Run_modeFayaoR = 2; //右发药
            break;
        default:
            //Run_modeFayaoL=0;
            //Run_modeFayaoR=0;
            ;
            break;
        }
        Run_mode = 0;
    }

    else if( (RecBuf1.act_buf.address1 >= 0x20 ) && (RecBuf1.act_buf.address1 <= 0x69) ) //向底层驱动板发送动作指令(合乎地址条件的）
    {
        Number_amout_2 = (RecBuf1.act_buf.address1 & 0x0F)
                         + ((((RecBuf1.act_buf.address1 & 0xE0) >> 5) & 0x07) - 1) * Floor_Number;

        CAN2TXbuffer1.act_buf.command = RecBuf1.act_buf.command;
//      TEST_COMMAND=RecBuf1.act_buf.command;
        CAN2TXbuffer1.act_buf.index1 = RecBuf1.act_buf.index1;
        CAN2TXbuffer1.act_buf.address1 = RecBuf1.act_buf.address1;
//		TEST_ADDRESS1=RecBuf1.act_buf.address1;
        CAN2TXbuffer1.act_buf.address2 = RecBuf1.act_buf.address2;
//		TEST_ADDRESS2=RecBuf1.act_buf.address2;
        CAN2TXbuffer1.act_buf.off_time = RecBuf1.act_buf.off_time;
        CAN2TXbuffer1.act_buf.on_time = RecBuf1.act_buf.on_time;
        CAN2TXbuffer1.act_buf.aim_amount = RecBuf1.act_buf.aim_amount;

        sum = 0;
        i = 0;
        do
        {
            sum += CAN2TXbuffer1.buf[i];
            i++;
        } while(i < 7);

        CAN2TXbuffer1.act_buf.checkout = sum;
        can2_transmit(CAN2TXbuffer1.act_buf.address1, CAN2TXbuffer1.buf);

        Set_DCT_Input_State(Number_amout_2, YES);
        Active_Data_Begin_Sign = YES;
    }
    else
    {

    }


}

//---------------------------------------------------------------------
//-----向上位机汇报底层驱动板的通讯及故障信息--------------------------
//-----不论驱动板工作是否正常，都上报----------
//都按照标准的一个机架16排计算，若有的机架，不够16排，通过上位机在界面上调整。
void Comunication_BITE_to_SWJ(void)
{
    static unsigned char  Number_amout = 0;
    unsigned char i, sum, bite_value, address_value;

    if( Number_amout >= Driver_Board_Amount ) Number_amout = 0;
    address_value = (Number_amout / Floor_Number) + 1;
    address_value = ((address_value << 5) & 0xE0) + (Number_amout % Floor_Number);

    if(SysData.DCT_Bite[Number_amout] >= 200)             //若2秒还没接受到此驱动板的应答，认为其有故障
        bite_value = 0x01;
    else    bite_value = 0x00;

    TransBuf1.amount_buf.command = 0xc0;
    TransBuf1.amount_buf.index1 =  Group_index_DCT;
    TransBuf1.amount_buf.address1 = address_value;        //机架号(从1开始)&&印制板的排号(从0开始)
    TransBuf1.amount_buf.address2 = 0x00;
    TransBuf1.amount_buf.bite =  bite_value;
    TransBuf1.amount_buf.blank1 =  0x00;
    TransBuf1.amount_buf.amount =  0x00;
    Uart1Send();

    Number_amout++;
}

void CAN2_Data_Collect_Process_Programme(void)
{
    unsigned char i, sum;
    //-------------------------------------------------------------------------
    //通过CAN2网络接受1号机柜底层主控板送过来的反馈数据1
    //分为两种类型数据，一种是故障电磁铁的实际动作次数，一种是故障状态或正常状态信息
    //--------------------------------------------------------------------------
    if(Can2NewData1 == YES)
    {
        sum = 0;
        i = 0;
        do
        {
            sum += CAN2RXbuffer1.buf[i];
            i++;
        } while(i < 7);

        if( ( sum == CAN2RXbuffer1.amount_buf.checkout )
                && ( CAN2RXbuffer1.amount_buf.command & 0xE0) == 0xA0 ) //电磁铁动作次数计数反馈指令
        {
            can_rx_from_DCT_amount_feedback(CAN2RXbuffer1.buf);
        }

        if( ( sum == CAN2RXbuffer1.amount_buf.checkout )
                && ( CAN2RXbuffer1.bite_buf.command & 0xE0) == 0xE0 ) //电磁铁BITE及OK信号反馈指令
        {
            can_rx_from_DCT_bite_feedback(CAN2RXbuffer1.buf);
        }

        Can2NewData1 = NO;
    }
    //-------------------------------------------------------------------
    //通过CAN2网络接受2号机柜底层主控板送过来的反馈数据1
    //分为两种类型数据，一种是故障电磁铁的实际动作次数，一种是故障状态或正常状态信息
    //-------------------------------------------------------------------
    if(Can2NewData2 == YES)
    {
        sum = 0;
        i = 0;
        do
        {
            sum += CAN2RXbuffer2.buf[i];
            i++;
        } while(i < 7);

        if( ( sum == CAN2RXbuffer2.amount_buf.checkout )
                && ( CAN2RXbuffer2.amount_buf.command & 0xE0) == 0xA0 ) //电磁铁动作次数计数反馈指令
        {
            can_rx_from_DCT_amount_feedback(CAN2RXbuffer2.buf);
        }

        if( ( sum == CAN2RXbuffer2.amount_buf.checkout )
                && ( CAN2RXbuffer2.bite_buf.command & 0xE0) == 0xE0 ) //电磁铁BITE及OK信号反馈指令
        {
            can_rx_from_DCT_bite_feedback(CAN2RXbuffer2.buf);
        }

        Can2NewData2 = NO;
    }
    //------------------------------------------------------------------------------
    //通过CAN2网络接受3号机柜底层主控板送过来的反馈数据1
    //分为两种类型数据，一种是故障电磁铁的实际动作次数，一种是故障状态或正常状态信息
    //------------------------------------------------------------------------------
    if(Can2NewData3 == YES)
    {
        sum = 0;
        i = 0;
        do
        {
            sum += CAN2RXbuffer3.buf[i];
            i++;
        } while(i < 7);

        if( ( sum == CAN2RXbuffer3.amount_buf.checkout )
                && ( CAN2RXbuffer3.amount_buf.command & 0xE0) == 0xA0 ) //电磁铁错误动作次数计数反馈指令
        {
            can_rx_from_DCT_amount_feedback(CAN2RXbuffer3.buf);
        }

        if( ( sum == CAN2RXbuffer3.amount_buf.checkout )
                && ( CAN2RXbuffer3.bite_buf.command & 0xE0) == 0xE0 ) //电磁铁BITE及OK信号反馈指令
        {
            can_rx_from_DCT_bite_feedback(CAN2RXbuffer3.buf);
        }

        Can2NewData3 = NO;
    }
}

//-------------------------------药品自动输送子程序--------------------------------
void init_machine(void)
{
    //
    Can_CHECKinfor(2, 0XFC);
    delay_ms(500);
    if((Can3NewData1 == YES) && (Mode_adress2 == 1))
    {
        if((CAN3RXbuffer1.buf[1] == 0xFC) || (CAN3RXbuffer1.buf[1] == 0xC3)) //药品提升初始化准备就绪判断
        {
            if(CAN3RXbuffer1.buf[5] != 0x00) //自动运行中报警
            {
                Lan_backalarm(2, 1, 14);
                Mode_ALARM2 = 1;
            }
            if(CAN3RXbuffer1.buf[4] == 0x00) //不在原点报警
            {
                Lan_backalarm(2, 1, 14);
                Mode_ALARM2 = 1;
            }
            if(CAN3RXbuffer1.buf[6] != 0x00) //报警位建立报警
            {
                Lan_backalarm(2, 1, 14);
                Mode_ALARM2 = 1;
            }
        }
        Can3NewData1 = NO;
        Mode_adress2 = 0;
    }
    //
    Can_CHECKinfor(3, 0XFC);
    delay_ms(500);
    if((Can3NewData1 == YES) && (Mode_adress3 == 1))
    {
        Can3NewData1 = NO;
        Mode_adress3 = 0;
        if((CAN3RXbuffer1.buf[1] == 0xFC) || (CAN3RXbuffer1.buf[1] == 0xC4)) //输送线
        {
					//暂时屏蔽
//            if((CAN3RXbuffer1.buf[4] & 0x01) != 0x01)
//            {
//                Lan_backalarm(3, 1, 12); //左翻板传感器不再原位报警
//                Mode_ALARM3 = 1;
//            }
            if((CAN3RXbuffer1.buf[4] & 0x04) != 0x04)
            {
                Lan_backalarm(3, 2, 13); //右翻板传感器不再原位报警
                Mode_ALARM3 = 1;
            }
//            if(CAN3RXbuffer1.buf[6] == 0x01) //报警位建立报警
//            {
//                Lan_backalarm(3, CAN3RXbuffer1.buf[2], 16);
//                Mode_ALARM3 = 1;
//            }
            /*if((CAN3RXbuffer1.buf[3]&0x01)==0x00)
            {
            	Lan_backalarm(3,0,52);//
            	Mode_ALARM3=1;
            }*/
        }
    }
    //
    Can_CHECKinfor(4, 0XFC);
    delay_ms(500);
    if((Can3NewData1 == YES) && (Mode_adress4 == 1))
    {
        if((CAN3RXbuffer1.buf[1] == 0xFC) || (CAN3RXbuffer1.buf[1] == 0xC3)) //药品提升初始化准备就绪判断
        {
            if(CAN3RXbuffer1.buf[5] != 0x00) //自动运行中报警
            {
                Lan_backalarm(4, 1, 14);
                Mode_ALARM4 = 1;
            }
            if(CAN3RXbuffer1.buf[4] == 0x00) //不在原点报警
            {
                Lan_backalarm(4, 1, 14);
                Mode_ALARM4 = 1;
            }
            if(CAN3RXbuffer1.buf[6] != 0x00) //报警位建立报警
            {
                Lan_backalarm(4, 1, 14);
                Mode_ALARM4 = 1;
            }
            //CAN3RXbuffer1.buf[5]=
        }
        Can3NewData1 = NO;
        Mode_adress4 = 0;
    }
}
//---------------------------------------------------------------------------------
//----------传送带状态检查，并发送左右0xCO程序段
//----------------------------------------------------------------------------------
void  Send_CO_Begin_Programme(void)
{

    if(((SNSORSTATUS1 & 0x01) == 0x01) && ((SNSORSTATUS2 & 0x01) == 0x01))
    {
        if(Run_ALARM1 == 0)
        {
            Lan_backinfor(0x08, 0x80); //左准备好
        }
        if(Run_ALARM2 == 0)
        {
            Lan_backinfor(0x09, 0x80); //右准备好
        }

    }

}


//-------------------------------药品自动输送子程序--------------------------------
void  Medica_Auto_Transportation_Programme(void)
{
    if(((SNSORSTATUS1 & 0x01) == 0x01) && ((SNSORSTATUS2 & 0x01) == 0x01))
    {
        if(ONCEFLAG == 0)
        {
            ONCEFLAG = 1;
            /*
            //delay_ms(500);
            if(Run_ALARM1==0)
            {
            	Lan_backinfor(0x08,0x80);//左准备好
            }
            if(Run_ALARM2==0)
            {
            	//delay_ms(500);
            	Lan_backinfor(0x09,0x80);//右准备好
            }
            */
        }
    }
    ///////////////////
    //输送线先行等待原点建立
    ///////////////////
    if(Run_ALARM1 == 0)
    {
        if( (Run_modeFayaoL == 0x01) && (Servo_Begin_Sign == YES) )
        {
            //ALARM_ONCEFLAGL=YES;
            //SNSORSTATUS2右翻遍建立标志SNSORSTATUS1左翻板建立标志MAINRUNSTATUSL1禁止运行标志
            if(((SNSORSTATUS1 & 0x01) == 0x01) && ((SNSORSTATUS2 & 0x01) == 0x01) && ((MAINRUNSTATUSL & 0X01) == 0X00)) //输送线左定位运行
            {
                SNSORSTATUS1 = SNSORSTATUS1 & 0xFE; //失效左翻板标志
                MAINRUNSTATUSL = MAINRUNSTATUSL | 0X01;
                ConveyingLineMovement();//输送线左定位运行
                AUTO_countimerL = 0;
                ALARM_ONCEFLAGL = 0;
            }
            if((SNSORSTATUS1 & 0X02) == 0x02) //药品提升原点建立
            {
                Run_modeFayaoL = 0;
                Run_REAFY1 = 0x01;//发药准备好
            }
            else
            {
                AUTO_countimerL = AUTO_countimerL + 1; //等待原点超时报警
                if(AUTO_countimerL > MAXZREO_TIME)
                {
                    Run_ALARM1 = 1;
                }
            }
        }
        ////////////////////////////
        /////////////////////////////
        if(Run_REAFY1 == 0x01) //左正常运行
        {
            // (SNSORSTATUS2&0x02)药品提升原点建立//定位运行结束
            if(((SNSORSTATUS1 & 0x02) == 0x02) && ((SNSORSTATUS1 & 0x10) == 0x10) && ((MAINRUNSTATUSL & 0X10) == 0X00)) //输送线右推药//(SNSORSTATUS2&0x02)==0x02提升机构原点   SNSORSTATUS1&0x10表示输送线定位运行结束
            {
                Alarm_stepflagL = 1;   //故障反向运行标志
                SNSORSTATUS1 = SNSORSTATUS1 & 0xEF; //清定位运行标志
                MAINRUNSTATUSL = MAINRUNSTATUSL | 0X10;
                //added start
//                ConveyingLongSlow_Stop();//地面输送线慢速模式结束
                FZSSXLeftFZ(0x01);//提升输送线翻转
                //added end
                Susongxiangtuiyao();//推药
            }
            if(((SNSORSTATUS1 & 0x02) == 0x02) && ((SNSORSTATUS1 & 0x20) == 0x20) && ((MAINRUNSTATUSL & 0X20) == 0X00)) //输送线药品提升  SNSORSTATUS1&0x20表示输送线推药结束
            {
                SNSORSTATUS1 = SNSORSTATUS1 & 0xDF;
                SNSORSTATUS1 = SNSORSTATUS1 | 0x01; //建立左翻板标志
                SNSORSTATUS1 = SNSORSTATUS1 & 0xFD; //失效药品提升原点标志
                MAINRUNSTATUSL = MAINRUNSTATUSL | 0X20;
                ONCEFLAG = 0;
                MAINRUNSTATUSL = MAINRUNSTATUSL & 0XFE; //左输送线定位运行放开
                Yaopingtishengleft();//
            }
            if(Runbiaoji3 == 1)
            {
                Runbiaoji3 = 0;
                Run_REAFY1 = 0x00;
                Alarm_stepflagL = 0;
                SNSORSTATUS1 = SNSORSTATUS1 | 0x02; //建立药品提升原点标志
                MAINRUNSTATUSL = MAINRUNSTATUSL & 0XEF; //左输送线推药运行
                MAINRUNSTATUSL = MAINRUNSTATUSL & 0XDF; //药品提升运行
            }
        }
    }
    else
    {
        Alarm_stepflagL = 0;
        Runbiaoji3 = 0;
        Run_REAFY1 = 0x00;
        SNSORSTATUS1 = 0x01;
        SNSORSTATUS1 = SNSORSTATUS1 | 0x02; //建立药品提升原点标志
        MAINRUNSTATUSL = MAINRUNSTATUSL & 0XFE; //左输送线定位运行放开
        MAINRUNSTATUSL = MAINRUNSTATUSL & 0XEF; //左输送线推药运行
        MAINRUNSTATUSL = MAINRUNSTATUSL & 0XDF; //药品提升运行
        Servo_left_Begin_Sign = 0;
    }

    ///////////////////
    //输送线先行等待原点建
    ///////////////////
    if(Run_ALARM2 == 0)
//    if(1)
    {
        if( (Run_modeFayaoR == 0x02) && (Servo_Begin_Sign == YES) )
        {
            //ALARM_ONCEFLAGR=YES;
            //SNSORSTATUS2右翻遍建立标志SNSORSTATUS1左翻板建立标志MAINRUNSTATUSL1禁止运行标志
            if(((SNSORSTATUS1 & 0x01) == 0x01) && ((SNSORSTATUS2 & 0x01) == 0x01) && ((MAINRUNSTATUSL1 & 0X01) == 0X00)) //输送线右定位运行
            {
                SNSORSTATUS2 = SNSORSTATUS2 & 0xFE; //失效右翻板标志
                MAINRUNSTATUSL1 = MAINRUNSTATUSL1 | 0X01;
							//暂时注释
//                ConveyingrightLineMovement();
                AUTO_countimerR = 0;
                ALARM_ONCEFLAGR = 0;
            }
            if((SNSORSTATUS2 & 0X02) == 0x02) //药品提升原点建立
            {
                Run_modeFayaoR = 0;
                Run_REAFY2 = 0x02;
            }
            else
            {
                AUTO_countimerR = AUTO_countimerR + 1;
                if(AUTO_countimerR > MAXZREO_TIME)
                {
                    Run_ALARM2 = 1;
                }
            }
        }
        ////////////////////////////
        /////////////////////////////
        if(Run_REAFY2 == 0x02) //右正常运行
        {
            // (SNSORSTATUS2&0x02)药品提升原点建立//定位运行结束
            if(((SNSORSTATUS2 & 0x02) == 0x02) && ((SNSORSTATUS2 & 0x10) == 0x10) && ((MAINRUNSTATUSL1 & 0X10) == 0X00)) //输送线右推药//(SNSORSTATUS2&0x02)==0x02提升机构原点
            {
                Alarm_stepflagR = 1;   //故障反向运行标志
                SNSORSTATUS2 = SNSORSTATUS2 & 0xEF;
                MAINRUNSTATUSL1 = MAINRUNSTATUSL1 | 0X10;
                //added start
                FZSSXRightFZ(0x01);//提升输送线反转
                //added end
                Susongxiangrighttuiyao();
            }
            if(((SNSORSTATUS2 & 0x02) == 0x02) && ((SNSORSTATUS2 & 0x20) == 0x20) && ((MAINRUNSTATUSL1 & 0X20) == 0X00)) //输送线药品提升
            {
                SNSORSTATUS2 = SNSORSTATUS2 & 0xDF;
                SNSORSTATUS2 = SNSORSTATUS2 | 0x01; //建立右翻板标志
                SNSORSTATUS2 = SNSORSTATUS2 & 0xFD; //失效药品提升原点标志
                MAINRUNSTATUSL1 = MAINRUNSTATUSL1 | 0X20;
                ONCEFLAG = 0;
                MAINRUNSTATUSL1 = MAINRUNSTATUSL1 & 0XFE; //右输送线定位运行放开
                Yaopingtisheng();
            }
            if(Runbiaoji4 == 1)
            {
                Runbiaoji4 = 0;
                Run_REAFY2 = 0x00;
                SNSORSTATUS2 = SNSORSTATUS2 | 0x02; //建立药品提升原点标志
                AUTO_countimerR = 0;
                MAINRUNSTATUSL1 = MAINRUNSTATUSL1 & 0XEF; //右输送线推药运行
                MAINRUNSTATUSL1 = MAINRUNSTATUSL1 & 0XDF; //药品提升运行
            }
        }
    }
    else
    {
        AUTO_countimerR = 0;
        Runbiaoji4 = 0;
        Run_REAFY2 = 0x00;
        SNSORSTATUS2 = 0x01;
        SNSORSTATUS2 = SNSORSTATUS2 | 0x02; //建立药品提升原点标志
        MAINRUNSTATUSL1 = MAINRUNSTATUSL1 & 0XFE; //右输送线定位运行放开
        MAINRUNSTATUSL1 = MAINRUNSTATUSL1 & 0XEF; //右输送线推药运行
        MAINRUNSTATUSL1 = MAINRUNSTATUSL1 & 0XDF; //药品提升运行
        Servo_right_Begin_Sign = 0;
    }


///故障处理//
    if((Run_ALARM1 == 1) && (Alarm_stepflagL == 0))
    {
        //可以反向运行
        //修改定位运行参数
        Servo_right_Begin_Sign = 1;
        if(ALARM_ONCEFLAGR == NO)
        {
            ALARM_ONCEFLAGR = !ALARM_ONCEFLAGR;
            if(Run_ALARM2 == 0)
            {
                m_floatServodisplace = 40000.0 * K + 0.5;
                Servodisplace.displace = (long)m_floatServodisplace;
                Servodisplace.displace = 0 - Servodisplace.displace;
                FAYAOinput_pra[0] = Servodisplace.buf[0];
                FAYAOinput_pra[1] = Servodisplace.buf[1];
                FAYAOinput_pra[2] = Servodisplace.buf[2];
                FAYAOinput_pra[3] = Servodisplace.buf[3];
                Run_modeFayaoR = 0x02;
            }
            else
            {
                ;
            }
        }
    }

    if((Run_ALARM2 == 1) && (Alarm_stepflagR == 0))
    {
        //可以反向运行
        //修改定位运行参数
        Servo_left_Begin_Sign = 1;
        if(ALARM_ONCEFLAGL == NO)
        {
            ALARM_ONCEFLAGL = !ALARM_ONCEFLAGL;
            if(Run_ALARM1 == 0)
            {
                m_floatServodisplace = 40000.0 * K + 0.5;
                Servodisplace.displace = (long)m_floatServodisplace;
                FAYAOinput_pra[0] = Servodisplace.buf[0];
                FAYAOinput_pra[1] = Servodisplace.buf[1];
                FAYAOinput_pra[2] = Servodisplace.buf[2];
                FAYAOinput_pra[3] = Servodisplace.buf[3];
                Run_modeFayaoL = 0x01;
            }
            else
            {
                ;
            }
        }
    }
} //  药品自动输送过程结束


//////////////////////////
//初始化变量
//////////////////////////
void init_para()
{
    unsigned char i;

    for(i = 0; i < 8; i++)
    {
        RecBuf1.buf[i] = 0;
        CAN2RXbuffer1.buf[i] = 0;
        CAN3RXbuffer1.buf[i] = 0;
    }
    for(i = 0; i < Driver_Board_Amount; i++)
    {
        SysData.DCT_Bite[i] = 0;        //统计底层驱动板的通讯及供电情况的BITE
    }

    for(i = 0; i < BYTE_Amount; i++)
    {
        SysData.DCT_Input_Command[i] = 0;
        SysData.DCT_OK_Result[i] = 0;
        SysData.DCT_OK_Action[i] = 0;
    }

    Procession1 = 0;
    Proce_Count = 0;

    OUTPUT1 = 1;  //光耦输出初始化
    OUTPUT2 = 1;
    OUTPUT3 = 1;
    OUTPUT4 = 1;

    RS485_EN = NO;   //默认是RS485接收数据
    NET_CFG = YES;   //默认网络插座为工作状态
    NET_RST = YES;   //高电平工作，低电平复位

    LED_BUF = 0xFF;
    SFRPAGE = CONFIG_PAGE ;
    Board_ID = (P5 & 0x1E) >> 1;

    //发药
    for(i = 0; i < 4; i++)
    {
        FAYAOinput_pra[i] = 0;
    }

    Board_Address_Number = 0; //自检用

    //默认零位已经完成，靠报警位是否可以运行
    SNSORSTATUS1 = 0x01 | 0x02;
    SNSORSTATUS2 = 0x01 | 0x02;
    MAINRUNSTATUSL = 0;
    MAINRUNSTATUSL1 = 0;
    communication_step = 0;
    Run_modeFayaoL = 0;
    Run_modeFayaoR = 0;
    Run_ALARM1 = 0; //初始化为报警状态
    Run_ALARM2 = 0; //初始化为报警状态
    Basket_stat = 0;
    CAN_index = 0;
    Alarm_stepflagL = NO;
    Alarm_stepflagR = NO;
    //Mode_adress=0;

    FaYao_Prosetion_Sign = NO;
    Active_Data_Begin_Sign_last = NO;
    Active_Data_Begin_Sign = NO;

}
//----------一个新的批次到来后的参数初始化-----------
void init_index_para()
{
    unsigned char i;

    ALL_Action_OK_Sign = 0;
    ALL_Result_OK_Sign = 0;
    ALL_Action_OK_Sign_last = 0;
    OK_Sign_TX_Amount = 0;
    Servo_Begin_Sign = NO;      //伺服触发标志清零
    Answer_ok = NO;

    FaYao_Prosetion_Sign = NO;
    Active_Data_Begin_Sign_last = NO;
    Active_Data_Begin_Sign = NO;

    for(i = 0; i < BYTE_Amount; i++)
    {
        SysData.DCT_Input_Command[i] = 0;
        SysData.DCT_OK_Result[i] = 0;
        SysData.DCT_OK_Action[i] = 0;
    }
}

//////////////////////////
//初始化硬件配置
//////////////////////////
void initialize()
{
    config();      //配置寄存器
    init_AD();    //初始化A/D
//	init_can1();   //初始化c8051f040自带CAN
    init_can2();   //初始化SJA1000_1
    init_can3();   //初始化SJA1000_2
    // init_GM8123(); //初始化串口控制器
    init_para();
}

//////////////////////////
//12位A/D采样
//输入：channel:通道号
//返回：0～4096 对应 0～2.43V
//////////////////////////
int get_ad_value(unsigned char channel)
{
    SFRPAGE = 0x00;
    AMX0SL = channel; //channel select
    AD0INT = 0; //清除转换结束标记
    AD0BUSY = 1; // 开始转换
    while (AD0INT == 0); // 等待转换结束
    return(ADC0); // 读ADC0数据
}

//////////////////////////
//读芯片温度
//返回：温度值（单位：度）
//////////////////////////
signed char get_temp()
{
    return( (signed char)((get_ad_value(0x0F) * 16 - 20928) / 77) );
}

//////////////////////////
//初始化A/D
//////////////////////////
void  init_AD()
{
    SFRPAGE = 0x00;
    REF0CN = 0x03;	// Reference Control Register
    ADC0CN |= 0xC0; // 使能ADC
    ADC0CF = (SYSCLK / 2500000) << 3; // ADC conversion clock = 2.5MHz
    REF0CN |= 0x04; //使能温度传感器
}

//---------------------------------------------------------------------------
//state=0: 灭 ;  state=1: 亮 。blink为YES取反操作，为NO按state状态操作
//---------------------------------------------------------------------------
void LedBlink(unsigned char num, unsigned char state, unsigned char blink )
{
    static unsigned char ledbuf1 = 0x00;
    if( blink == NO )
    {
        if(state == 0)
        {
            ledbuf1 = ledbuf1 & ( ~(0x01 << (num - 1)) );
        }
        else
        {
            ledbuf1 = ledbuf1 | (0x01 << (num - 1)) ;
        }
    }
    else
    {
        ledbuf1 = ledbuf1 ^ (0x01 << (num - 1)) ;
    }
    LED_BUF = ~ledbuf1;
}

//
void Lan_backalarm(unsigned char ID, unsigned char MUM, unsigned char m_alarm1)
{
    unsigned char i = 0;
    unsigned char sum = 0;
    TransBuf1.amount_buf.command = 22;
    TransBuf1.amount_buf.index1 = 22;
    TransBuf1.amount_buf.address1 = ID;
    TransBuf1.amount_buf.address2 = MUM;
    TransBuf1.amount_buf.bite = 0x00;
    TransBuf1.amount_buf.blank1 = m_alarm1;
    TransBuf1.amount_buf.amount = 0x00;
    sum = 0;
    for(i = 0; i < 7; i++)
    {
        sum = sum + TransBuf1.buf[i];
    }
    TransBuf1.amount_buf.checkout = sum;
    Uart1Send();
}

//
void Lan_backinfor(unsigned char m_pra1, unsigned char m_pra2)
{
    unsigned char i = 0;
    unsigned char sum = 0;
    TransBuf1.amount_buf.command = m_pra1;
    TransBuf1.amount_buf.index1 = 0x00;
    TransBuf1.amount_buf.address1 = 0xAF;
    TransBuf1.amount_buf.address2 = m_pra2;
    TransBuf1.amount_buf.bite = 0x00;
    TransBuf1.amount_buf.blank1 = 0x00;
    TransBuf1.amount_buf.amount = 0x00;
    TransBuf1.amount_buf.amount = 0x00;

    sum = 0;
    for(i = 0; i < 7; i++)
    {
        sum = sum + TransBuf1.buf[i];
    }
    TransBuf1.amount_buf.checkout = sum;
    Uart1Send();
}

/*************************************CAN3发送函数段****************************************
*
*
********************************************************************************************/
//---------------------- --------------------自动发药程序
//发送对象：随ID
void Can_CHECKinfor(unsigned char ID, unsigned char command)
{
    unsigned char i, sum;
    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = command;
    CAN3TXbuffer1.trans.index = 0x01;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(ID, CAN3TXbuffer1.buf);
}

//电磁铁控制
//发送对象：1
void ElectromagnetCRTL(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFE;
    CAN3TXbuffer1.trans.index = 0x01;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(1, CAN3TXbuffer1.buf);
}

//药篮步进运行
//发送对象：1
void YaolanStepRun(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFD;
    CAN3TXbuffer1.trans.index = 0x01;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(1, CAN3TXbuffer1.buf);
}

//药篮提升
//发送对象：2
void Yaolantisheng(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFE;
    CAN3TXbuffer1.trans.index = 0x00;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(2, CAN3TXbuffer1.buf);
}

//---------------------- --------------------自动发药程序
//输送线运动
//发送对象：3
void ConveyingLineMovement(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFE;
    CAN3TXbuffer1.trans.index = 0x01;
    CAN3TXbuffer1.trans.data1 = FAYAOinput_pra[0];
    CAN3TXbuffer1.trans.data2 = FAYAOinput_pra[1];
    CAN3TXbuffer1.trans.data3 = FAYAOinput_pra[2];
    CAN3TXbuffer1.trans.data4 = FAYAOinput_pra[3];
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(3, CAN3TXbuffer1.buf);
}

//输送线运动
//发送对象：3
void ConveyingrightLineMovement(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFE;
    CAN3TXbuffer1.trans.index = 0x02;
//    CAN3TXbuffer1.trans.data1 = FAYAOinput_pra[0];
//    CAN3TXbuffer1.trans.data2 = FAYAOinput_pra[1];
//    CAN3TXbuffer1.trans.data3 = FAYAOinput_pra[2];
//    CAN3TXbuffer1.trans.data4 = FAYAOinput_pra[3];
    CAN3TXbuffer1.trans.data1 = 0;
    CAN3TXbuffer1.trans.data2 = 0;
    CAN3TXbuffer1.trans.data3 = 3;
    CAN3TXbuffer1.trans.data4 = 0xe8;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(3, CAN3TXbuffer1.buf);
}

//输送箱退药
//发送对象：3
void Susongxiangtuiyao(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFD;
    CAN3TXbuffer1.trans.index = 0x01;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(3, CAN3TXbuffer1.buf);
}

//
//发送对象：3
void Susongxiangrighttuiyao(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFD;
    CAN3TXbuffer1.trans.index = 0x02;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(3, CAN3TXbuffer1.buf);
}

//左侧药品提升
//发送对象：2
void Yaopingtishengleft(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFE;
    CAN3TXbuffer1.trans.index = CAN_index;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(2, CAN3TXbuffer1.buf);
}

//右侧药品提升
//发送对象：4
void Yaopingtisheng(void)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xFE;
    CAN3TXbuffer1.trans.index = CAN_index;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(4, CAN3TXbuffer1.buf);
}

//added start
//
//启动地面输送线无限长、慢速运行
//发送对象：3
//dir:1-输送线左转、2-输送线右转
void ConveyingLongSlow_Start(unsigned char dir)
{
    unsigned char i, sum;

    CAN3TXbuffer1.trans.address1 = 0x01;
    CAN3TXbuffer1.trans.command = 0xB2;
    CAN3TXbuffer1.trans.index = dir;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(DM_CANID, CAN3TXbuffer1.buf);
}

//停止地面输送线无限长、慢速运行
//发送对象：3
void ConveyingLongSlow_Stop()
{
    unsigned char i, sum;

    CAN3TXbuffer1.trans.address1 = 0x01;
    CAN3TXbuffer1.trans.command = 0xB3;
    CAN3TXbuffer1.trans.index = 0x00;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(DM_CANID, CAN3TXbuffer1.buf);
}

//右侧翻转输送线反转
//发送对象：2
//signal:1-start、2-stop
void FZSSXRightFZ(unsigned char signal)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xB0;
    CAN3TXbuffer1.trans.index = signal;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(YTS_CANID, CAN3TXbuffer1.buf);
}

//左侧翻转输送线反转
//发送对象：4
//signal:1-start、2-stop
void FZSSXLeftFZ(unsigned char signal)
{
    unsigned char i, sum;

    //----------------向底层驱动板发送准备好指令---------------------------------
    CAN3TXbuffer1.trans.address1 = 0X01;
    CAN3TXbuffer1.trans.command = 0xB0;
    CAN3TXbuffer1.trans.index = signal;
    CAN3TXbuffer1.trans.data1 = 0X00;
    CAN3TXbuffer1.trans.data2 = 0X00;
    CAN3TXbuffer1.trans.data3 = 0X00;
    CAN3TXbuffer1.trans.data4 = 0X00;
    sum = 0;
    i = 0;
    do
    {
        sum += CAN3TXbuffer1.buf[i];
        i++;
    } while(i < 7);
    CAN3TXbuffer1.trans.checkout = sum;
    can3_transmit(ZTS_CANID, CAN3TXbuffer1.buf);
}

//added end


/*************************************CAN3发送函数段结束************************************
*
*
********************************************************************************************/


//-----------------------------------------------------------------------------
//自定义延时
//延时时间约为(us N10)毫秒
//-----------------------------------------------------------------------------
void delay1( unsigned int us)
{
    unsigned int i = us;
    while(i--) ;
}
/**************************************************************************/
void delay_ms(unsigned int ms)
{
    unsigned int us = 1000;
    while(ms--)
    {
        WDTCN = 0xA5;
        delay1(us);
    }
}

//-----------------------------------------------------------------------
//////////////////////////
// 主程序
//////////////////////////
void main(void)
{
    unsigned char i, sum, Action_OK_Num;
    initialize();
    delay_ms(2000);          //为了底层机构顺利初始化，将延时由以前的4000ms改为15000ms   20160719
    init_machine();

//	Lan_backinfor(0x08,0x80);//左准备好     //仅供发药系统调试用，正式要删除
//  Lan_backinfor(0x09,0x80);//右准备好     //仅供发药系统调试用，正式要删除

    while(1)
    {
        //程序运行指示灯
        if(T0Counter1 >= 15)
        {
            T0Counter1 = 0 ;
            LedBlink(1, 1, 1 );
        }

        /*****************************************************************
        一、CAN1口或网络口通讯
        1.通过CAN1通讯接受上位机送过来的指令性数据
        2.通过网络通讯接受上位机送过来的指令性数据
        3.主要为1.自检指令，2.发药电磁铁动作指令，3.传送带闸板选择指令
        *****************************************************************/
        //-------------------------------------------------------------------------
        //                        接受上位机的控制指令
        //--------------------------------------------------------------------------
        if( UART1_Refresh == YES )
        {
            sum = 0;
            i = 0;
            do
            {
                sum += RecBuf1.buf[i];
                i++;
            } while(i < 7); //校验

            if( sum == RecBuf1.act_buf.checkout )
            {
                /*****************根据指令执行动作*******************/
                if( (RecBuf1.act_buf.command & 0x1C) == 0x04 )    //自检指令
                {
                    can_rx_from_SWJ_Zijian_Command();
                }
                else  if( (RecBuf1.act_buf.command & 0xF0) == 0x40 ) //0XC0请求包接收处理
                {
                    if( (RecBuf1.act_buf.address1 == 0xbe) && (RecBuf1.act_buf.address2 == 0x77) )
                    {
                        Send_CO_Begin_Programme();
                    }
                    else
                    {
                    }
                }


                if( (RecBuf1.act_buf.command & 0xFF) == 0x11 )    //
                {
                    Servo_Begin_Sign = YES;

//										Run_modeFayaoL = 1;
//                    Servo_left_Begin_Sign = YES;

                    Run_modeFayaoR = 2;
                    Servo_right_Begin_Sign = YES;

                }


                else  if( (RecBuf1.act_buf.command & 0xF0) == 0x20 ) //发药指令，包含始发包，结束包接收处理
                {
                    can_rx_from_SWJ_Action_Command();
                }
                else if( (RecBuf1.act_buf.command & 0xF0) == 0x30 ) //机械动作数据包
                {
                    //Lan_backinfor(0x66);
                    switch(RecBuf1.buf[4])
                    {
                    case 1:
                        Run_mode = 1;
                        break;
                    case 2:
                        Run_mode = 2;
                        break;
                    default:
                        break;
                    }
                    if(Run_mode == 2)
                    {
                        switch(RecBuf1.buf[5])
                        {
                        case 1:
                            CAN_index = 0;
                            break;
                        case 2:
                            CAN_index = 1;
                            break;
                        case 3:
                            CAN_index = 2;
                            break;
                        case 4:
                            CAN_index = 3;
                            break;
                        case 5:
                            CAN_index = 4;
                            break;
                        default:
                            CAN_index = 4;
                            break;
                        }
                    }
                    Servodisplace.buf[0] = 0;
                    Servodisplace.buf[1] = 0;
                    Servodisplace.buf[2] = 0;
                    Servodisplace.buf[3] = 0;

                    m_intServodisplace = RecBuf1.buf[2];
                    m_intServodisplace = m_intServodisplace << 8;
                    m_intServodisplace = m_intServodisplace + RecBuf1.buf[3];
                    m_floatServodisplace = (float)m_intServodisplace * K + 0.5;
                    Servodisplace.displace = (long)m_floatServodisplace;

                    switch(Run_mode)
                    {
                    case 1:
                        FAYAOinput_pra[0] = Servodisplace.buf[0];
                        FAYAOinput_pra[1] = Servodisplace.buf[1];
                        FAYAOinput_pra[2] = Servodisplace.buf[2];
                        FAYAOinput_pra[3] = Servodisplace.buf[3];
                        break;
                    case 2:
                        Servodisplace.displace = 0 - Servodisplace.displace;
                        FAYAOinput_pra[0] = Servodisplace.buf[0];
                        FAYAOinput_pra[1] = Servodisplace.buf[1];
                        FAYAOinput_pra[2] = Servodisplace.buf[2];
                        FAYAOinput_pra[3] = Servodisplace.buf[3];
                        break;
                    default:
                        break;
                    }
                    communication_step = 2;
                }
                else
                {
                }
                /*****************************************************/
            }
            UART1_Refresh = NO;
        }

        //-------------------------------------------------------------------------
        //  向上位机汇报底层驱动板的通讯及供电工作状况
        //--------------------------------------------------------------------------

        if(T0Counter3 >= 20)                 //每隔200ms向上位机发送通讯BITE信息
        {
            T0Counter3 = 0 ;
            if( Zijian_Sign == 0x55 )
            {
                Comunication_BITE_to_SWJ();    //向上位机发送底层驱动板的通讯BITE数据信息
            }
            else
            {

            }
        }

        //-------------------------------------------------------------------------
        //                        向上位机汇报底层驱动板发药的BITE状态
        //--------------------------------------------------------------------------

        if(T0Counter4 >= 10)  //100ms回路控制，对OK信息的处理和上报
        {
            T0Counter4 = 0;

            //-------------------底层驱动板送过来的动作OK信息汇总处理----------------------
            //在指令结束后，才启动判断流程
            //在动作指令下发给底层驱动板后,感觉没必要，所以剔除了
            if( Command_Finish_Sign == YES )         //现支持6个药架，共96个排
            {
                Action_OK_Num = 0;
                for(i = 0; i < BYTE_Amount; i++)
                {
                    if( ( SysData.DCT_Input_Command[i] == SysData.DCT_OK_Result[i] ) )
                        Action_OK_Num = Action_OK_Num + 1;
                    else   Action_OK_Num = Action_OK_Num + 0;
                }
                if( Action_OK_Num == BYTE_Amount )	 ALL_Result_OK_Sign = 0xAA;
                else                                 ALL_Result_OK_Sign = 0x00;

                //-------------------底层驱动板送过来的动作结束信息汇总处理----------------------
                Action_OK_Num = 0;
                for(i = 0; i < BYTE_Amount; i++)
                {
                    if( ( SysData.DCT_Input_Command[i] == SysData.DCT_OK_Action[i] ) )
                        Action_OK_Num = Action_OK_Num + 1;
                    else   Action_OK_Num = Action_OK_Num + 0;
                }
                if( Action_OK_Num == BYTE_Amount )	 ALL_Action_OK_Sign = 0xBB;
                else                                 ALL_Action_OK_Sign = 0x00;
            }

            if(   (FaYao_Prosetion_Sign == YES)
                    && (T0Counter15 >= Bite_Delay_Time)
              )
            {
                T0Counter15 = 0;
                ALL_Result_OK_Sign = 0x55;
                ALL_Action_OK_Sign = 0xBB;
                FaYao_Prosetion_Sign = NO;
            }
            /*######################################################################### */
            if( (ALL_Action_OK_Sign == 0xBB)
                    //  &&(T0Counter14>=50)
              )       //电磁铁发药结束后，延时1秒开始触发伺服动作
            {
                //		T0Counter14 =51;
                Servo_Begin_Sign = YES;       //伺服开始标志
                FaYao_Prosetion_Sign = NO;
            }
            else {}
            //-------当动作都结束后，向上位机上报，且只上报3次结果信息------------
            //-------此上报在电磁铁发药结束后，还是在伺服动作完成后，需要商讨
            //暂时屏蔽
            if( (ALL_Action_OK_Sign == 0xBB) && ( OK_Sign_TX_Amount < 3 ) )
            {
                TransBuf1.OK_buf.command = 0xE0;
                TransBuf1.OK_buf.index1 = Group_index_DCT;
                TransBuf1.OK_buf.blank1 = 0x00;
                TransBuf1.OK_buf.blank2 = 0x00;
                TransBuf1.OK_buf.ALL_Result_OK_Sign = ALL_Result_OK_Sign;
                TransBuf1.OK_buf.ALL_Action_OK_Sign = ALL_Action_OK_Sign;
                TransBuf1.OK_buf.blank3 = 0x00;
                Uart1Send();
                OK_Sign_TX_Amount++;
            }
        }    //100ms定时程序结束

        /*****************************************************************
        二、CAN2口通讯
        1.通过CAN1通讯接受上位机送过来的指令性数据-------------
        2.通过网络通讯接受上位机送过来的指令性数据-------------
        3.主要为1，自检指令，2.发药电磁铁动作指令，3.传送带闸板选择指令
        *****************************************************************/
        CAN2_Data_Collect_Process_Programme();

        /*****************************************************************
        三、CAN3通道用于驱动控制药品传输机构，包括传送带、药框管理、药框提升等
        并适时采集上报机构状态信息
        *****************************************************************/
        /*--------------------------------------------------------------------
        1、采用CAN3口向药品输送机构发送工作指令
        ----------------------------------------------------------------------*/
        // Medica_Auto_Transportation_Programme();   //药品自动输送
        /*--------------------------------------------------------------------
        2、采用CAN3口接收药品输送机构反馈回来的状态信息及故障信息
        ----------------------------------------------------------------------*/
        if(Can3NewData1 == YES)
        {
            sum = 0;
            i = 0;
            do
            {
                sum += CAN3RXbuffer1.buf[i];
                i++;
            } while(i < 7);

            if( ( sum == CAN3RXbuffer1.rec.checkout )
                    && ( CAN3RXbuffer1.rec.command == 0x18 )
              )
            {
                Answer_ok = YES;
            }
            //暂时屏蔽了Mode_adress2
//            if(Mode_adress2 == 1)
//            {
//                if(CAN3RXbuffer1.buf[1] == 0xC4)
//                {
//                    Lan_backalarm(2, 1, 8);
//                    Mode_ALARM2 = 1;
//                }
//                if(CAN3RXbuffer1.buf[1] == 0xC2)
//                {
//                    if((CAN3RXbuffer1.buf[3] & 0x08) == 0x08)
//                    {
//                        Lan_backalarm(2, 1, 6);
//                        Mode_ALARM2 = 1;
//                    }
//                }
//                if((CAN3RXbuffer1.buf[1] == 0xFC) || (CAN3RXbuffer1.buf[1] == 0xC3)) //药品提升初始化准备就绪判断
//                {
//                    //注意报警清零条件
//                    if(CAN3RXbuffer1.buf[1] == 0xC3) {
//                        ONCEFLAG = 0;
//                    }
//                    if(CAN3RXbuffer1.buf[5] != 0x00) //自动运行中报警
//                    {
//                        Lan_backalarm(2, 1, 14);
//                        Mode_ALARM2 = 1;
//                    }
//                    if(CAN3RXbuffer1.buf[4] == 0x00)   //不在原点报警
//                    {
//                        Lan_backalarm(2, 1, 14);
//                        Mode_ALARM2 = 1;
//                    }
//                    if(CAN3RXbuffer1.buf[6] != 0x00) //报警位建立报警
//                    {
//                        Lan_backalarm(2, 1, 14);
//                        Mode_ALARM2 = 1;
//                    }
//                    if(((CAN3RXbuffer1.buf[3] & 0x08) != 0x08) && (CAN3RXbuffer1.buf[5] == 0x00) && (CAN3RXbuffer1.buf[6] == 0x00) && (CAN3RXbuffer1.buf[4] == 0x01))
//                    {
//                        Mode_ALARM2 = 0;
//                        Alarm_stepflagL = 0;
//                    }

//                    //CAN3RXbuffer1.buf[5]=
//                }
//                if(CAN3RXbuffer1.buf[1] == 0xE0)
//                {
//                    Mode_ALARM2 = 1;
//                }

//                Mode_adress2 = 0;
//            }
            if(Mode_adress3 == 1)
            {
                Mode_adress3 = 0;
                if((CAN3RXbuffer1.buf[1] == 0xFC) || (CAN3RXbuffer1.buf[1] == 0xC4)) //药品提升初始化准备就绪判断
                {
                    if(CAN3RXbuffer1.buf[1] == 0xC4) {
                        ONCEFLAG = 0;
                    }

                    if((CAN3RXbuffer1.buf[4] & 0x01) != 0x01)
                    {
                        Lan_backalarm(3, 1, 12); //左翻板传感器不再原位报警
                        Mode_ALARM3 = 1;

                    }
                    if((CAN3RXbuffer1.buf[4] & 0x04) != 0x04)
                    {
                        Lan_backalarm(3, 2, 13); //右翻板传感器不再原位报警
                        Mode_ALARM3 = 1;
                    }
                    if(CAN3RXbuffer1.buf[6] == 0x01) //报警位建立报警
                    {
                        Lan_backalarm(3, CAN3RXbuffer1.buf[2], 16);
                        Mode_ALARM3 = 1;
                    }
                    if(((CAN3RXbuffer1.buf[4] & 0x01) == 0x01) && ((CAN3RXbuffer1.buf[4] & 0x04) == 0x04) && (CAN3RXbuffer1.buf[6] == 0x00))
                    {
                        Mode_ALARM3 = 0;
                    }

                }
                if(CAN3RXbuffer1.buf[1] == 0xE0)
                {
                    Lan_backalarm(3, CAN3RXbuffer1.buf[2], 1); //输送线左上翻报错
                    Mode_ALARM3 = 1;
                }
                if(CAN3RXbuffer1.buf[1] == 0xE1)
                {
                    Lan_backalarm(3, CAN3RXbuffer1.buf[2], 2); //输送线左下翻报错
                    Mode_ALARM3 = 1;
                }
                if(CAN3RXbuffer1.buf[1] == 0xFD)
                {
                    if((CAN3RXbuffer1.buf[3] & 0x01) == 0x00)
                    {
                        Lan_backalarm(3, 1, 3);
                    }
                    if((CAN3RXbuffer1.buf[3] & 0x02) == 0x00)
                    {
                        Lan_backalarm(3, 2, 3);
                    }
                }
            }
            if(Mode_adress4 == 1)
            {
                if(CAN3RXbuffer1.buf[1] == 0xC4)
                {
                    Lan_backalarm(4, 2, 9);
                    Mode_ALARM4 = 1;
                }
                if(CAN3RXbuffer1.buf[1] == 0xC2)
                {
                    if((CAN3RXbuffer1.buf[3] & 0x08) == 0x08)
                    {
                        Lan_backalarm(4, 2, 7);
                        Mode_ALARM4 = 1;
                    }
                }

                if((CAN3RXbuffer1.buf[1] == 0xFC) || (CAN3RXbuffer1.buf[1] == 0xC3)) //药品提升初始化准备就绪判断
                {
                    //注意报警清零条件
                    if(CAN3RXbuffer1.buf[1] == 0xC3) {
                        ONCEFLAG = 0;
                    }
                    if(CAN3RXbuffer1.buf[5] != 0x00) //自动运行中报警
                    {
                        Lan_backalarm(4, 2, 14);
                        Mode_ALARM4 = 1;
                    }
                    if(CAN3RXbuffer1.buf[4] == 0x00) //不在原点报警
                    {
                        Lan_backalarm(4, 2, 14);
                        Mode_ALARM4 = 1;
                    }
                    if(CAN3RXbuffer1.buf[6] != 0x00) //报警位建立报警
                    {
                        Lan_backalarm(4, 2, 14);
                        Mode_ALARM4 = 1;
                    }
                    if(((CAN3RXbuffer1.buf[3] & 0x08) != 0x08) && (CAN3RXbuffer1.buf[5] == 0x00) && (CAN3RXbuffer1.buf[6] == 0x00) && (CAN3RXbuffer1.buf[4] == 0x01))
                    {
                        Mode_ALARM4 = 0;
                        Alarm_stepflagL = 0;
                    }

                    //CAN3RXbuffer1.buf[5]=
                }
                if(CAN3RXbuffer1.buf[1] == 0xE0)
                {
                    Mode_ALARM4 = 1;
                }

                Mode_adress4 = 0;
            }
            Can3NewData1 = NO;
        }
        /*if(Run_modeFayao==1)
        {
        //Run_modeFayao=0;
        Medica_Auto_Transportation_Programme();
        }*/
        if((Mode_ALARM4 == 1) || (Mode_ALARM3 == 1))
        {
            Run_ALARM2 = 1;
        }
        else
        {
            Run_ALARM2 = 0; //维修后故障清除报警自动清除
        }
        if((Mode_ALARM1 == 1) || (Mode_ALARM2 == 1) || (Mode_ALARM3 == 1))
        {
            Run_ALARM1 = 1;
        }
        else
        {
            Run_ALARM1 = 0;
        }

        //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
        if(Servo_Begin_Sign == YES)     //触发伺服机构运行
        {

            if( Run_modeFayaoL == 1) //左发药
            {
                Servo_left_Begin_Sign = YES;
            }
            if(Run_modeFayaoR == 2) //右发药
            {
                Servo_right_Begin_Sign = YES;
            }
        }
        else
        {

        }

        if( (Servo_left_Begin_Sign == YES) || (Servo_right_Begin_Sign == YES))
        {
            Medica_Auto_Transportation_Programme();
        }

        /*****************************************************************
        	三个CAN通道初始化
        *****************************************************************/
        CAN_AUTO_RESET();     //CAN接口自动复位程序
    }

}
