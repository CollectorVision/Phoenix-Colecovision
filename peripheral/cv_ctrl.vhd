--
-- ColecoVision Controllers with interrupt output and quadrature inputs.
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


-- Controllers with interrupt and quadrature inputs.
--
-- The ground path for the switches is provided via a single logic output which
-- must be capable of sinking enough current for at least five (5) simultaneous
-- closed switches; 16mA to 24mA is probably required.
--
-- The direction is from the perspective of the circuit, i.e. an "input" is a
-- signal from the controller to the circuit.
--
-- ^ means the pin has a weak pull-up on the input.
-- * pin-9 has a resistor in the SAC itself that acts as a pull-up when the
--   stick is selected, and a pull-down when the keypad is selected. The
--   resistor is 10K and is between pin-5 and pin-8, which are the low (ground)
--   paths for the switches.
--
-- Controller DB9 Socket (male pins on console)
-- ================================
-- Pin   Dir    Stick       Keypad
--             CV (SAC)    CV (SAC)
-- --------------------------------
--  1^   in    Up          P1
--  2^   in    Down        P2
--  3^   in    Left        P3
--  4^   in    Right       P4
--  5    out   High        Low
--  6^   in    Fire Left   Fire/Arm Right
--  7^   in    High (QB)   High
--  8    out   Low         High
--  9*   in    Low  (QA)   Low (!QA - inverse of when stick is selected)
--
-- Controller Input Data Byte
-- ===============================
--  7   6   5   4   3   2   1   0   bit
--  ------------------------------
--  0   FL  1   1   L   D   R   U   stick   \ Standard
--  0   FR  1   1   P3  P2  P4  P1  keypad  / Controller
--  ------------------------------
--  QA  FL  QB  IF  L   D   R   U   stick   \ Super Action
--  !QA FR  1   IF  P3  P2  P4  P1  keypad  / Controller
--  ------------------------------
--  x   6   7   x   3   2   4   1   DB9 pin controlling the input bit
--
-- Input bits 7 and 4 are derived from other circuitry and not directly from
-- controller inputs.
--
-- QA - quadrature A
-- QB - quadrature B
-- FL - left fire button
-- FR - right fire button
-- IF - interrupt flag
--
--
-- Lookup table for keypad button to input data value in low 4-bits
-- of controller data.  The input line is low when a button is pushed.
--
--           3 2 4 1   DB9-pin
--      key| 8 4 2 1 | bit-value
--      ---|---------|----
--       1 | 1 1 0 1 | >0D ~>02
--       2 | 0 1 1 1 | >07 ~>08
--       3 | 1 1 0 0 | >0C ~>03
--       4 | 0 0 1 0 | >02 ~>0D
--       5 | 0 0 1 1 | >03 ~>0C
--       6 | 1 1 1 0 | >0E ~>01
--       7 | 0 1 0 1 | >05 ~>0A
--       8 | 0 0 0 1 | >01 ~>0E
--       9 | 1 0 1 1 | >0B ~>04
--       0 | 1 0 1 0 | >0A ~>05
--       * | 1 0 0 1 | >09 ~>06
--       # | 0 1 1 0 | >06 ~>09
-- SAC Pur | 1 0 0 0 | >08 ~>07
-- SAC Blu | 0 1 0 0 | >04 ~>0B
-- none    | 1 1 1 1 | >0F ~>00
-- invalid | 0 0 0 0 | >00 ~>0F
--
--
-- Quadrature Circuits
-- ===================
--
-- Note: All timing and voltages were measured from an actual ColecoVision
--       PCB, Rev F, 1982.
--
-- Controller input pin-9 is falling-edge detected and inverted via a NAND
-- gate (U24 74LS00) and associated circuitry.  The NAND output becomes the
-- QA data input, and also triggers a one-shot with an RC period that creates
-- the active-low Interrupt Flag bit.  The Interrupt Flag (IF) duration is about
-- 550uS in stick mode, and 450uS in keypad mode (measured).  The one-shot RC
-- circuit also triggers the interrupt RC circuit to create a 20uS active-low
-- interrupt signal to the CPU.  The maskable ISR can check each controller's
-- Interrupt Flag bit (as long as the ISR reads the data within ~500uS of the
-- interrupt signal) to determine which (or both) of the controllers triggered
-- the interrupt.
--
-- The quadrature circuits are active during stick *and* keypad mode, however
-- in keypad mode the QB input is pulled high via a 10K resistor in the SAC,
-- so even though the interrupt will be generated, the direction cannot be
-- determined because the QB input is always high in keypad mode.
--
-- Switching from stick->keypad or keypad->stick modes can cause an interrupt
-- because the QA switch changes from being pulled-up to pulled-down, which
-- looks like a transition without the switch actually opening or closing.
--
-- However, the pin-9 input is passed through an RC filter (R19 & C17) prior
-- to being applied to the input of the NAND gate.  This effectively slows down
-- the input transition such that is takes ~400uS for the signal to cause the
-- input to the NAND to change.  Thus, any transition from stick to keypad, or
-- vice versa, that takes less than ~400uS will *not* cause an interrupt.
--
-- Because the interrupt is only generated on the rising-edge of the NAND
-- output (QA), the QA vs QB phase at the time of the interrupt is always
-- the same:
--
-- Dir  QA  QB
-- ===========
-- CW    1   1
-- CCW   1   0
--
-- To capture the other phases of QA vs QB, the controller data would have to
-- be sampled during periods between the interrupts.
--
-- During a very fast spin of the SAC wheel, a period of about 6ms for each
-- quadrature input was measured.  The duty-cycle is 2:1 High:Low.
-- __________ _____________ ___  ___
--           V             V     INT
-- ____ 2ms  |_______      |___
--     \____/|  4ms  \____/|     QA bit-7
--    ____   |      ____   |              CCW
-- __/    \__|_____/    \__|___  QB bit-5
-- __________| ____________| ___
--           \/500uS low   \/    IF bit-4
--           |             |
-- ____      |_______      |___
--     \____/|       \____/|     QA bit-7
--         __|_          __|_             CW
-- _______/  | \________/  | \__ QB bit-5
-- __________| ____________| ___
--           \/            \/    IF bit-4


library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;


-- Input synchronizer for controller pins other than pin-9.
--
entity cv_ctrl_sync is
port (
   clk_i          : in  std_logic;
   clk_en_i       : in  std_logic;
   d_i            : in  std_logic;
   d_o            : out std_logic
);
end cv_ctrl_sync;

architecture rtl of cv_ctrl_sync is

   signal sync_r, sync_x   : std_logic_vector(3 downto 0) := "0000";
   signal d_r, d_x         : std_logic := '0';

begin

   sync_x <= sync_r(2 downto 0) & d_i;
   d_x <=
      '0' when sync_r = "0000" else
      '1' when sync_r = "1111" else
      d_r;

   process ( clk_i, clk_en_i )
   begin
      if rising_edge(clk_i) and clk_en_i = '1' then
         sync_r <= sync_x;
         d_r    <= d_x;
      end if;
   end process;

   -- Output
   d_o <= d_r;

end rtl;


library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;


-- Pin-9 synchronizer and RC-filter emulation.
--
-- The counters are designed for a specific input clock frequency.
--
entity cv_ctrl_pin9 is
port (
   clk_i          : in  std_logic;  --  21.47727 (21.739) MHz
   clk_en_i       : in  std_logic;
   d_i            : in  std_logic;
   d_o            : out std_logic
);
end cv_ctrl_pin9;

architecture rtl of cv_ctrl_pin9 is

   signal sync_r, sync_x            : std_logic_vector(3 downto 0) := "0000";
   signal d_r, d_x                  : std_logic := '0';

   signal rc_rise_r, rc_rise_x      : unsigned(10 downto 0) := (others => '0');
   signal rc_rise_zero_s            : std_logic;
   signal rc_rise_en_s              : std_logic;

   signal rc_fall_r, rc_fall_x      : unsigned(8 downto 0) := (others => '0');
   signal rc_fall_zero_s            : std_logic;
   signal rc_fall_en_s              : std_logic;

begin

   -- The pin-9 input is first passed through an RC-filter to slow down the
   -- controller switch input transitions.  The delay is ~400uS before the U24
   -- NAND sees a 0->1 transition at ~1.0V.  The 1->0 transition takes ~140uS
   -- at ~1.2V.
   sync_x <= sync_r(2 downto 0) & d_i;

   -- While the input is 0, keep the rise timer loaded.
   -- Count is 400uS / 279ns = 1433 = "101 1001 1001"
   rc_rise_zero_s <= '1' when rc_rise_r = 0 else '0';
   rc_rise_en_s <= '1' when sync_r = "0000" or rc_rise_zero_s = '0' else '0';
   rc_rise_x <= "10110011001" when sync_r = "0000" else rc_rise_r - 1;

   -- While the input is 1, keep the fall timer loaded.
   -- Count is 140uS / 279ns = 502 = "1 1111 0110"
   rc_fall_zero_s <= '1' when rc_fall_r = 0 else '0';
   rc_fall_en_s <= '1' when sync_r = "1111" or rc_fall_zero_s = '0' else '0';
   rc_fall_x <= "111110110" when sync_r = "1111" else rc_fall_r - 1;

   d_x <=
      '0' when rc_fall_zero_s = '1' and rc_rise_zero_s = '0' else
      '1' when rc_fall_zero_s = '0' and rc_rise_zero_s = '1' else
      d_r;

   process ( clk_i, clk_en_i )
   begin
      if rising_edge(clk_i) and clk_en_i = '1' then
         sync_r <= sync_x;
         d_r    <= d_x;

         if rc_rise_en_s = '1' then
            rc_rise_r <= rc_rise_x;
         end if;

         if rc_fall_en_s = '1' then
            rc_fall_r <= rc_fall_x;
         end if;
      end if;
   end process;

   -- Output
   d_o <= d_r;

end rtl;


library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;


-- Input change detection.
--
entity cv_ctrl_change_detect is
port (
   clk_i          : in  std_logic;  --  21.47727 (21.739) MHz
   clk_en_i       : in  std_logic;  --  3.58MHz enable
   reset_n_i      : in  std_logic;
   d_i            : in  std_logic;
   d_o            : out std_logic
);
end cv_ctrl_change_detect;

architecture rtl of cv_ctrl_change_detect is

   signal d_r, d_x                  : std_logic := '1'; -- default to floating high
   signal d_last_r                  : std_logic := '0'; -- default to low
   signal d_cnt_r, d_cnt_x          : unsigned(1 downto 0) := "00";
   signal d_follow_s                : std_logic;

begin

   -- Any controller that does not use the quadrature inputs, i.e. the standard
   -- CV controllers, will most likely leave inputs 7 and 9 disconnected.  This
   -- is a problem for pin-9 since it ties directly to an R/C input filter and
   -- then to the input of a NAND gate:
   --
   --                          ______
   --                      1 --)12   \
   -- pin-9 ----/\/\/\----+----)13  11)O--- 0
   --             R19     |    ------/
   --                    ---
   --                    --- C17
   --                     |
   --                     V
   --
   -- When left disconnected, the input to the NAND gate will be floating, and
   -- since it is a TTL input, it will float to a logic level of one.  This will
   -- cause the output of the NAND gate to be low, and subsequently the input
   -- D7-bit of the controller data byte will always be low (zero).
   --
   -- This creates two problems.
   --
   -- First, it is very difficult to emulate this floating-input behavior since
   -- it is not possible to detect when a real controller is connected.  A real
   -- connection to pin-9 could be high or low at any given time.
   --
   -- Second, some poorly written software includes bit-7 in its controller
   -- status logic, and thus will not work correctly if bit-7 is other than 0,
   -- which correlates to a high input on pin-13 of the NAND gate.  This also
   -- means the poorly written software will not work correctly on a real CV
   -- when a real SAC is connected!
   --
   -- The way detection is going to be done here is to watch the input for any
   -- change, and if it does change, allow following of the input.  Otherwise,
   -- the input will be treated as a logic high.
   
   d_follow_s <= d_cnt_r(1);
   
   -- Count changes to the input, clamp at 2 changes.
   d_cnt_x <= d_cnt_r when d_last_r = d_i or d_follow_s = '1' else d_cnt_r + 1;

   -- Follow the input or keep initial value.
   d_x <= d_r when d_follow_s = '0' else d_i;

   
   process ( clk_i, clk_en_i )
   begin
   if rising_edge(clk_i) and clk_en_i = '1' then
      
      if reset_n_i = '0' then
         d_r      <= '1';  -- reset to floating high
         d_last_r <= '0';
         d_cnt_r  <= "00";
      else
         d_r      <= d_x;
         d_last_r <= d_i;
         d_cnt_r  <= d_cnt_x;
      end if;
   
   end if;
   end process;

   -- Output
   d_o <= d_r;

end rtl;


-- Main CV controller circuit.
--
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

-- CV Controller
--
entity cv_ctrl is
port (
   clk_21m47_i       : in  std_logic;  --  21.47727 (21.739) MHz
   clk_en_3m58_i     : in  std_logic;
   reset_n_i         : in  std_logic;
   ctrl_en_key_n_i   : in  std_logic;
   ctrl_en_joy_n_i   : in  std_logic;
   ctrl_p1_i         : in  std_logic_vector(2 downto 1);
   ctrl_p2_i         : in  std_logic_vector(2 downto 1);
   ctrl_p3_i         : in  std_logic_vector(2 downto 1);
   ctrl_p4_i         : in  std_logic_vector(2 downto 1);
   ctrl_p5_o         : out std_logic;
   ctrl_p6_i         : in  std_logic_vector(2 downto 1);
   ctrl_p7_i         : in  std_logic_vector(2 downto 1);
   ctrl_p8_o         : out std_logic;
   ctrl_p9_i         : in  std_logic_vector(2 downto 1);
   ctrl_sel_i        : in  std_logic;  -- A1=0 read controller 1, A1=1 read controller 2
   ctrl_d_o          : out std_logic_vector(7 downto 0);
   ctrl_int_n_o      : out std_logic
);
end cv_ctrl;

architecture rtl of cv_ctrl is

   signal keyjoy_sel_r, keyjoy_sel_x   : std_logic := '0';

   -- Synchronization
   signal p1_1_r,  p2_1_r              : std_logic := '0';
   signal p1_2_r,  p2_2_r              : std_logic := '0';
   signal p1_3_r,  p2_3_r              : std_logic := '0';
   signal p1_4_r,  p2_4_r              : std_logic := '0';
   signal p1_F_r,  p2_F_r              : std_logic := '0';
   signal p1_QB_r, p2_QB_r             : std_logic := '0';
   signal p1_9f_r, p2_9f_r             : std_logic := '0';
   signal p1_9_r,  p2_9_r              : std_logic := '1'; -- default to floating

   -- Quadrature and interrupt
   signal p1_QA_s, p2_QA_s             : std_logic;
   signal p1_edge_s, p2_edge_s         : std_logic;
   signal p1_9last_r, p2_9last_r       : std_logic := '0';
   signal cnt_zero_s                   : std_logic;
   signal cnt_load_s                   : std_logic;
   signal cnt_en_n_s                   : std_logic;
   signal intcnt_r, intcnt_x           : unsigned(6 downto 0) := "0000000";
   signal int_n_r                      : std_logic := '1';

   -- Interrupt flag bits
   signal p1_IF_r                      : std_logic := '1';
   signal p1_ifc_cnt_r, p1_ifc_cnt_x   : unsigned(10 downto 0) := "11100000000";
   signal p1_ifc_zero_s                : std_logic;
   signal p1_ifc_load_s                : std_logic;

   signal p2_IF_r                      : std_logic := '1';
   signal p2_ifc_cnt_r, p2_ifc_cnt_x   : unsigned(10 downto 0) := "11100000000";
   signal p2_ifc_zero_s                : std_logic;
   signal p2_ifc_load_s                : std_logic;

begin

   -- R/S Flip-Flop to provide a ground to the keypad or stick switches.
   -- If the keypad select is low, pin-5 will be set low and remain low.
   -- If the stick select is low, pin-8 will be low and remain low.
   ctrl_p5_o <= keyjoy_sel_r;
   ctrl_p8_o <= not keyjoy_sel_r;

   keyjoy_sel_x <=
      '0' when ctrl_en_key_n_i = '0' and ctrl_en_joy_n_i = '1' else
      '1' when ctrl_en_key_n_i = '1' and ctrl_en_joy_n_i = '0' else
      keyjoy_sel_r;

   process ( clk_21m47_i, clk_en_3m58_i )
   begin
      if rising_edge(clk_21m47_i) and clk_en_3m58_i = '1' then
         if reset_n_i = '0' then
            keyjoy_sel_r <= '0';
         else
            keyjoy_sel_r <= keyjoy_sel_x;
         end if;
      end if;
   end process;

   -- Synchronize the inputs and provide a little debounce (about 1uS).
   p1_1 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p1_i(1), p1_1_r);
   p1_2 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p2_i(1), p1_2_r);
   p1_3 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p3_i(1), p1_3_r);
   p1_4 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p4_i(1), p1_4_r);
   p1_6 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p6_i(1), p1_F_r);
   p1_7 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p7_i(1), p1_QB_r);
   p1_9 : entity work.cv_ctrl_pin9 port map (clk_21m47_i, clk_en_3m58_i, ctrl_p9_i(1), p1_9f_r);
   p1_9cd : entity work.cv_ctrl_change_detect port map (clk_21m47_i, clk_en_3m58_i, reset_n_i, p1_9f_r, p1_9_r);

   p2_1 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p1_i(2), p2_1_r);
   p2_2 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p2_i(2), p2_2_r);
   p2_3 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p3_i(2), p2_3_r);
   p2_4 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p4_i(2), p2_4_r);
   p2_6 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p6_i(2), p2_F_r);
   p2_7 : entity work.cv_ctrl_sync port map (clk_21m47_i, clk_en_3m58_i, ctrl_p7_i(2), p2_QB_r);
   p2_9 : entity work.cv_ctrl_pin9 port map (clk_21m47_i, clk_en_3m58_i, ctrl_p9_i(2), p2_9f_r);
   p2_9cd : entity work.cv_ctrl_change_detect port map (clk_21m47_i, clk_en_3m58_i, reset_n_i, p2_9f_r, p2_9_r);

   
   -- Create QA.
   p1_QA_s <= p1_9_r nand p1_IF_r;
   p2_QA_s <= p2_9_r nand p2_IF_r;

   -- Falling edge detect pin-9.
   p1_edge_s <= p1_QA_s and p1_9last_r;
   p2_edge_s <= p2_QA_s and p2_9last_r;

   -- Interrupt pulse duration zero-compare.
   cnt_zero_s <= '1' when intcnt_r = 0 else '0';

   -- Interrupt counter load enable from each controller.  Blocked if the
   -- counter is not zero, i.e. already applying the interrupt pulse.
   cnt_load_s <= (p1_edge_s or p2_edge_s) and cnt_zero_s;

   -- Interrupt count enable.  Enabled if the counter is not zero, or if the
   -- counter needs to be loaded due to a falling-edge.
   cnt_en_n_s <= (p1_edge_s nor p2_edge_s) and cnt_zero_s;

   -- Interrupt counter next state mux.  Decrement or load.
   -- Count is 20uS / 279.36nS = 71.5
   -- Period for 3.58MHz is 279nS.  72 = 1001000
   intcnt_x <= "1001000" when cnt_load_s = '1' else intcnt_r - 1;

   -- Per-controller Interrupt Flag counters.
   -- Count is 500uS / 279nS = 1792 = "111 0000 0000"
   -- The flag goes low when an interrupt is triggered.  In stick
   -- mode the duration is 550uS, in keypad mode it is 450uS, so just
   -- use 500uS for both.
   p1_ifc_zero_s <= '1' when p1_ifc_cnt_r = 0 else '0';
   p1_ifc_load_s <= p1_edge_s and p1_ifc_zero_s;
   p1_ifc_cnt_x <= "11100000000" when p1_ifc_load_s = '1' else p1_ifc_cnt_r - 1;

   p2_ifc_zero_s <= '1' when p2_ifc_cnt_r = 0 else '0';
   p2_ifc_load_s <= p2_edge_s and p2_ifc_zero_s;
   p2_ifc_cnt_x <= "11100000000" when p2_ifc_load_s = '1' else p2_ifc_cnt_r - 1;

   process ( clk_21m47_i, clk_en_3m58_i )
   begin
   if rising_edge(clk_21m47_i) and clk_en_3m58_i = '1' then
      if reset_n_i = '0' then
         intcnt_r       <= (others => '0');
         int_n_r        <= '1';
         p1_ifc_cnt_r   <= (others => '0');
         p2_ifc_cnt_r   <= (others => '0');
         p1_IF_r        <= '1';
         p2_IF_r        <= '1';
         p1_9last_r     <= '0';
         p2_9last_r     <= '0';
      else
      
         -- If the interrupt duration counter is enabled, count down or load.
         if cnt_en_n_s = '0' then
            intcnt_r <= intcnt_x;
         end if;

         -- P1 interrupt flag counter.
         if p1_ifc_zero_s = '0' or p1_ifc_load_s = '1' then
            p1_ifc_cnt_r <= p1_ifc_cnt_x;
         end if;

         -- P2 interrupt flag counter.
         if p2_ifc_zero_s = '0' or p2_ifc_load_s = '1' then
            p2_ifc_cnt_r <= p2_ifc_cnt_x;
         end if;

         p1_IF_r     <= p1_ifc_zero_s;
         p2_IF_r     <= p2_ifc_zero_s;

         int_n_r     <= cnt_zero_s;
         p1_9last_r  <= p1_9_r;
         p2_9last_r  <= p2_9_r;
      end if;
   end if;
   end process;

   -- Controller Input Data Byte
   -- ===============================
   --  7   6   5   4   3   2   1   0   bit
   --  ------------------------------
   --  0   FL  1   1   L   D   R   U   stick   \ Standard
   --  0   FR  1   1   P3  P2  P4  P1  keypad  / Controller
   --  ------------------------------
   --  QA  FL  QB  IF  L   D   R   U   stick   \ Super Action
   --  !QA FR  1   IF  P3  P2  P4  P1  keypad  / Controller
   --  ------------------------------
   --  x   6   7   x   3   2   4   1   DB9 pin controlling the input bit
   --
   -- Input bits 7 and 4 are derived from other circuitry and not directly from
   -- controller inputs.
   --
   -- QA - quadrature A
   -- QB - quadrature B
   -- FL - left fire button
   -- FR - right fire button
   -- IF - interrupt flag

   -- Output
   -- A1=0 read controller 1, A1=1 read controller 2
   ctrl_d_o <=
      p1_QA_s & p1_F_r & p1_QB_r & p1_IF_r &
      p1_3_r  & p1_2_r & p1_4_r  & p1_1_r
   when ctrl_sel_i = '0'
   else
      p2_QA_s & p2_F_r & p2_QB_r & p2_IF_r &
      p2_3_r  & p2_2_r & p2_4_r  & p2_1_r;

   ctrl_int_n_o <= int_n_r;

end rtl;
