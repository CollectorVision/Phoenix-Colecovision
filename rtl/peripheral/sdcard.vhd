--
-- SD-card SPI with two-speed clock support and CPU-wait signal.
--

-- Released under the 3-Clause BSD License:
--
-- Copyright 2019 Matthew Hagerty (matthew <at> dnotq <dot> io)
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
--
-- 1. Redistributions of source code must retain the above copyright notice,
-- this list of conditions and the following disclaimer.
--
-- 2. Redistributions in binary form must reproduce the above copyright
-- notice, this list of conditions and the following disclaimer in the
-- documentation and/or other materials provided with the distribution.
--
-- 3. Neither the name of the copyright holder nor the names of its
-- contributors may be used to endorse or promote products derived from this
-- software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
-- POSSIBILITY OF SUCH DAMAGE.

--
-- MMC cards have a maximum clock of 25MHz, and a maximum clock of 400KHz is
-- required during the initialization phase, until the card type and speed are
-- known and a higher speed negotiated.  The Phoenix uses single-bit SPI mode:
--
--   MMC SPI  20MHz  2.5MB/s
--   SD  SPI  25MHz  2.125MB/s
--
-- The Phoenix system clock is around 25MHz, which is divided by two in the
-- SPI FSM to derive the SPI clock.  This easily stays under the 20MHz max
-- clock to support MMC.  When in slow-clock mode, the input clock is divided
-- by 64 to create a clock below the required 400KHz.
--
-- Still, these clocks are fast enough to read or write a byte at the typical
-- speed of the Z80; and able to read or write an entire 512K memory in about 1
-- second.
--
-- Assuming a system clock of 25MHz, the resulting clock speeds and Z80 I/O
-- instruction comparison:
--
--         Clock     Period    8-bit transfer
--       ------------------------------------
-- Slow  390.62KHz    2.56us   23.52us
-- Fast   12.5MHz    80.00ns  640.00ns
-- Z80     3.58MHz  279.36ns    1.11us (4-cycle) I/O instruction
--
-- Using the fast clock, an 8-bit transfer should be fast enough to only cause
-- one additional wait-state during a normal IO instruction, which takes four
-- machine cycles (T-states) minimum on a Z80.
--
--
-- SPI Mode 0 is implemented:
--
-- Clock Polarity (CPOL):  0 idle low
-- Clock Phase    (CPHA):  0 sample on rising edge
-- Data transfer        :  MSbit first
--
--
-- The host system has separate control of the SD-card chip-select, allowing
-- the host to transfer multiple bytes in a single operation, which is required
-- for many SPI commands.  A typical sequence would be:
--
-- 1. Set SD-card cs_n low
-- 2. Output command
-- 3. Output arg 1
-- 4. Output arg 2
-- 5. Output arg 3
-- 6. Output arg 4
-- 7. Output CRC
-- 8. Set SD-card cs_n high
--
-- The separate chip-select also facilitates sending clock pulses only, which
-- is an initialization requirement for SD-cards.
--
-- The ability of the SPI master (the host computer in this case) to stop the
-- clock, yet maintain chip-select low, allows the host CPU to read or write
-- multiple bytes using individual IN / OUT instructions.
--
-- The host system should pull the cs_n_i low for the duration of the 8-bit
-- operation, and can be thought of as the chip-select for the memory.  The
-- wait_n_o output will go low for the duration of the operation.
--
-- A CPU OUT instruction will write a byte to the SD-card, and an IN
-- instruction will read a byte from the SD-card.  Since SPI is a full-duplex
-- protocol, 8-bits are always written to, and read from, the slave for every
-- operation.  During a read operation, a dummy byte of >FF will be written to
-- the SD-card.
--
-- Example Z80 timing when using the fast clock.  Note that the Z80
-- automatically includes 1 wait-state during an IO operation, and the SPI
-- transfer should only cause one additional wait state when using the fast
-- clock.
--
--  |    T1   |    T2   |    TW   | SPI WAIT|    T3   |
--  |____     |____     |____     |____     |____     |__
-- _/    \____/    \____/    \____/    \____/    \____/    3.58MHz
-- _|_________|_________|_________|_________|_________| _
-- _X__________________________________________________X_  Port Address
-- _|_________|_        |         |         |     ____|__
--              \________________________________/         IOR_n
-- _|_________|_        |         |         |     ____|__
--              \________________________________/         RD_n     \
--  |         |         |         |         |   ___   |              } Read cycle
-- --------------------------------------------X___X-----  Data In  /
--  |         |         |  ___    |         |         |
-- XXXXXXXXXXXXXXXXXXXXXXX/   \XXXXXXXXXXXXXXXXXXXXXXXXXX  CPU imposed wait
--  |         |     ____|_________|__       |         |
-- XXXXXXXXXXXXXXXX/                 \XXXXXXXXXXXXXXXXXXX  SPI wait state
-- _|_________|_        |         |         |     ____|__
--              \________________________________/         WR_n     \
--  |      ___|_________|_________|_________|_________|              } Write cycle
-- -------X____________________________________________X-  Data Out /
--  |         |         |         |         |         |
-- VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV  system clock, 25MHz max
-- \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/  fast SPI clock, system clock/2
-- _|_________|_    | | | | | | | |         |_________|__
--              \__________________________/               cs_n_i (IOR_n + IO decode)
-- _|_________|___  | | | | | | | |  _______|_________|__
--                \_________________/                      wait_n_o (to CPU)
-- _|_________|_____|_|_|_|_|_|_|_|_________|_________|__
-- ________________X_X_X_X_X_X_X_X_______________________  SPI 8-bit bidirectional transfer
--  |         |   _                 ________|_________|__
-- XXXXXXXXXXXXXXX_XXXXXXXXXXXXXXXXX_____________________  Data-in and data-out
--                ^                 ^
--  CPU data-in latched          data valid for CPU read
--  for SPI MOSI data            from SPI MISO data
--
-- The slow clock operation extends the SPI 8-bit transfer time and keeps the
-- hold_n_o signal low, which will cause the CPU to wait.
--
-- The slow clock must be used to initialize the SD-card to SPI mode.
-- After that, the fast clock can be used.
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

entity sdcard is
port
   ( clk_i              : in     std_logic -- 25MHz max
   ; reset_n_i          : in     std_logic -- active low

   -- SD-card control
   ; slow_clk_i         : in     std_logic -- '1' = use slow SPI clock
   ; spi_ss_n_i         : in     std_logic -- '0' to select the SD-card device

   -- CPU
   ; cs_n_i             : in     std_logic -- '0' start an 8-bit transfer, hold low until wait_n_o = '1'
   ; wait_n_o           : out    std_logic -- '0' during transfer, CPU should wait
   ; wr_n_i             : in     std_logic -- '0' for write, allows >FF to be written during a read
   ; data_i             : in     std_logic_vector( 7 downto 0)
   ; data_o             : out    std_logic_vector( 7 downto 0)

   -- SD-card SPI
   ; spi_cs_n_o         : out    std_logic
   ; spi_sclk_o         : out    std_logic
   ; spi_mosi_o         : out    std_logic
   ; spi_miso_i         : in     std_logic
);
end entity;

architecture rtl of sdcard is

   type state_t is (st_idle, st_clk1, st_clk0, st_wait_eoc);
   signal state_r, state_x : state_t;

   signal rt_en_s                : std_logic := '1';

   signal dmux_s                 : std_logic_vector( 7 downto 0) := x"00";
   signal shift_r, shift_x       : std_logic_vector( 7 downto 0) := x"00";
   signal count_r, count_x       : unsigned( 2 downto 0) := "111";
   signal din_r, din_x           : std_logic := '0';

   signal wait_n_r, wait_n_x     : std_logic := '1';

   signal spi_ss_n_r             : std_logic := '1';
   signal clk_r, clk_x           : std_logic := '0';
   signal clk_en_s               : std_logic := '1';

   -- Slow clock divides the input clock by 32.
   signal slow_clk_r, slow_clk_x : unsigned( 4 downto 0) := "00000";
   signal slow_en_s              : std_logic := '0';


begin

   -- SC-card SPI outputs.
   spi_cs_n_o <= spi_ss_n_r;
   spi_sclk_o <= clk_r;

   -- The output data is always the MSb of the data.
   -- For SD-cards, always drive the MOSI.  Might need to change if used
   -- for other SPI interfaces, i.e. tri-state.
   spi_mosi_o <= shift_r(7);

   -- Data to the CPU and wait signal.
   data_o   <= shift_r;
   wait_n_o <= wait_n_r;

   -- Always write >FF on MOSI during a read operation.  The SD-card sees this
   -- values as a dummy byte and will ignore it.
   dmux_s <= data_i when wr_n_i = '0' else (others => '1');

   process ( state_r, shift_r, count_r, din_r, clk_r, dmux_s,
             cs_n_i, data_i, spi_miso_i )
   begin

      state_x     <= state_r;
      shift_x     <= shift_r;
      count_x     <= count_r;
      din_x       <= din_r;

      clk_x       <= clk_r;         -- clock hold state
      wait_n_x    <= '1';           -- not waiting
      slow_en_s   <= '0';           -- not a slow clock state

      case state_r is
      when st_idle =>

         clk_x          <= '0';     -- clock idle low

         if cs_n_i = '0' then
            state_x     <= st_clk1;
            wait_n_x    <= '0';     -- wait until 8-bit transfer is done
            shift_x     <= dmux_s;  -- load data from CPU during write
            count_x     <= "111";   -- counter load not necessary, but just in case
            slow_en_s   <= '1';     -- slow clock can be enabled
         end if;

      when st_clk1 =>

         state_x     <= st_clk0;
         slow_en_s   <= '1';        -- slow clock can be enabled
         wait_n_x    <= '0';        -- keep waiting
         clk_x       <= '1';        -- clock high
         din_x       <= spi_miso_i; -- latch input data

      when st_clk0 =>

         state_x     <= st_clk1;
         slow_en_s   <= '1';        -- slow clock can be enabled
         clk_x       <= '0';        -- clock low
         wait_n_x    <= '0';

         -- Shift the data during clock zero.
         shift_x  <= shift_r(6 downto 0) & din_r;
         count_x  <= count_r - 1;

         if count_r = 0 then
            state_x     <= st_wait_eoc;
         end if;

      when st_wait_eoc =>

         -- Wait for the end of this CPU IO cycle.
         if cs_n_i = '1' then
            state_x <= st_idle;
         end if;

      end case;

   end process;


   process ( slow_clk_i, slow_en_s, slow_clk_r )
   begin
      rt_en_s     <= '1';           -- enable register-transfer
      clk_en_s    <= '1';           -- no clock delay
      slow_clk_x  <= slow_clk_r;

      if slow_clk_i = '1' and slow_en_s = '1' then
         if slow_clk_r /= 0 then
            rt_en_s <= '0';
         end if;

         if slow_clk_r /= 1 then
            clk_en_s <= '0';
         end if;

         -- Divide the input clock to produce the slow clock.
         slow_clk_x <= slow_clk_r - 1;
      end if;
   end process;


   process (clk_i)
   begin
      if rising_edge(clk_i) then
         if reset_n_i = '0' then

            state_r     <= st_idle;
            slow_clk_r  <= (others => '0');
            shift_r     <= (others => '1');
            count_r     <= (others => '1');  -- reset to max makes loading unnecessary above
            din_r       <= '0';

            wait_n_r    <= '1';
            spi_ss_n_r  <= '1';
            clk_r       <= '0';
         else

            -- Limit register-transfer based on the slow clock setting
            -- and the current state.
            if rt_en_s = '1' then
               state_r  <= state_x;
               shift_r  <= shift_x;
               count_r  <= count_x;
            end if;

            -- Delay the clock to place the transition in the middle of
            -- the state, when using the slow clock.
            if clk_en_s = '1' then
               clk_r    <= clk_x;
            end if;

            slow_clk_r  <= slow_clk_x;
            din_r       <= din_x;

            wait_n_r    <= wait_n_x;
            spi_ss_n_r  <= spi_ss_n_i;

         end if;
      end if;
   end process;

end rtl;
