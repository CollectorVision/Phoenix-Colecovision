-------------------------------------------------------------------------------
--
-- SNESpad controller core
--
-- $Id: snespad_pad.vhd,v 1.2 2005/09/15 17:28:02 arniml Exp $
--
-- Copyright (c) 2004, Arnim Laeuger (arniml@opencores.org)
--
-- All rights reserved
--
-- Redistribution and use in source and synthezised forms, with or without
-- modification, are permitted provided that the following conditions are met:
--
-- Redistributions of source code must retain the above copyright notice,
-- this list of conditions and the following disclaimer.
--
-- Redistributions in synthesized form must reproduce the above copyright
-- notice, this list of conditions and the following disclaimer in the
-- documentation and/or other materials provided with the distribution.
--
-- Neither the name of the author nor the names of other contributors may
-- be used to endorse or promote products derived from this software without
-- specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
-- THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
-- PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
-- POSSIBILITY OF SUCH DAMAGE.
--
-- Please report bugs to the author, but before you do so, please
-- make sure that this is not a derivative work and that
-- you have the latest version of this file.
--
-- The latest version of this file can be found at:
--      http://www.opencores.org/cvsweb.shtml/gamepads/
--
-- The project homepage is located at:
--      http://www.opencores.org/projects.cgi/web/gamepads/overview
--
-------------------------------------------------------------------------------

--
-- Retaining the above copyright for attribution.
--
-- Release as a 3-clause BSD license with permission exlusive to CollectorVision Phoenix Project
-- otherwise reverts to above GPL3 license. See License.PHX file for more info
--

library ieee;
use ieee.std_logic_1164.all;

entity snespad_pad is

  generic (
    reset_level_g   :     natural := 0;
    button_level_g  :     natural := 0
  );
  port (
    -- System Interface -------------------------------------------------------
    clk_i           : in  std_logic;
    reset_i         : in  std_logic;
    clk_en_i        : in  boolean;
    -- Control Interface ------------------------------------------------------
    shift_buttons_i : in  boolean;
    save_buttons_i  : in  boolean;
    -- Pad Interface ----------------------------------------------------------
    pad_data_i      : in  std_logic;
    -- Buttons Interface ------------------------------------------------------
    but_a_o         : out std_logic;
    but_b_o         : out std_logic;
    but_x_o         : out std_logic;
    but_y_o         : out std_logic;
    but_start_o     : out std_logic;
    but_sel_o       : out std_logic;
    but_tl_o        : out std_logic;
    but_tr_o        : out std_logic;
    but_up_o        : out std_logic;
    but_down_o      : out std_logic;
    but_left_o      : out std_logic;
    but_right_o     : out std_logic;
    but_0_o         : out std_logic;
    but_1_o         : out std_logic;
    but_2_o         : out std_logic;
    but_3_o         : out std_logic;
    but_4_o         : out std_logic;
    but_5_o         : out std_logic;
    but_6_o         : out std_logic;
    but_7_o         : out std_logic;
    but_8_o         : out std_logic;
    but_9_o         : out std_logic;
    but_star_o      : out std_logic;
    but_num_o       : out std_logic;
    but_dot_o       : out std_logic;
    but_clear_o     : out std_logic;
    but_equal_o     : out std_logic
  );

end snespad_pad;


use work.snespad_pack.all;

architecture rtl of snespad_pad is

  signal buttons_q,
         shift_buttons_q : buttons_t;

begin

  -- pragma translate_off
  -----------------------------------------------------------------------------
  -- Check generics
  -----------------------------------------------------------------------------
  assert (reset_level_g = 0) or (reset_level_g = 1)
    report "reset_level_g must be either 0 or 1!"
    severity failure;

  assert (button_level_g = 0) or (button_level_g = 1)
    report "button_level_g must be either 0 or 1!"
    severity failure;
  -- pragma translate_on

  seq: process (clk_i)
  begin
  if rising_edge(clk_i) then
    if reset_i = reset_level_g then
      for i in buttons_t'range loop
        buttons_q(i)       <= button_reset_f(button_level_g);
        shift_buttons_q(i) <= button_reset_f(button_level_g);
      end loop;

    else
      if save_buttons_i then
			if  shift_buttons_q(but_pos_bit3_c) = 1
			and shift_buttons_q(but_pos_bit2_c) = 0
			and shift_buttons_q(but_pos_bit1_c) = 1
			and shift_buttons_q(but_pos_bit0_c) = 1
		then
			buttons_q <= shift_buttons_q;
		else
			buttons_q <= shift_buttons_q(31 downto 16) & "1111111111111111";
		end if;
	end if;

      if clk_en_i and shift_buttons_i then
        shift_buttons_q(buttons_t'high downto 1) <= shift_buttons_q(buttons_t'high-1 downto 0);
        shift_buttons_q(0)                       <= button_active_f(pad_data_i, button_level_g);
      end if;

    end if;
  end if;
  end process;


  -----------------------------------------------------------------------------
  -- Output Mapping
  -----------------------------------------------------------------------------
  but_a_o     <= buttons_q(but_pos_a_c);
  but_b_o     <= buttons_q(but_pos_b_c);
  but_x_o     <= buttons_q(but_pos_x_c);
  but_y_o     <= buttons_q(but_pos_y_c);
  but_start_o <= buttons_q(but_pos_start_c);
  but_sel_o   <= buttons_q(but_pos_sel_c);
  but_tl_o    <= buttons_q(but_pos_tl_c);
  but_tr_o    <= buttons_q(but_pos_tr_c);
  but_up_o    <= buttons_q(but_pos_up_c);
  but_down_o  <= buttons_q(but_pos_down_c);
  but_left_o  <= buttons_q(but_pos_left_c);
  but_right_o <= buttons_q(but_pos_right_c);
  but_0_o     <= buttons_q(but_pos_0_c);
  but_1_o     <= buttons_q(but_pos_1_c);
  but_2_o     <= buttons_q(but_pos_2_c);
  but_3_o     <= buttons_q(but_pos_3_c);
  but_4_o     <= buttons_q(but_pos_4_c);
  but_5_o     <= buttons_q(but_pos_5_c);
  but_6_o     <= buttons_q(but_pos_6_c);
  but_7_o     <= buttons_q(but_pos_7_c);
  but_8_o     <= buttons_q(but_pos_8_c);
  but_9_o     <= buttons_q(but_pos_9_c);
  but_star_o  <= buttons_q(but_pos_star_c);
  but_num_o   <= buttons_q(but_pos_num_c);
  but_dot_o   <= buttons_q(but_pos_dot_c);
  but_clear_o <= buttons_q(but_pos_clear_c);
  but_equal_o <= buttons_q(but_pos_equal_c);
end rtl;


-------------------------------------------------------------------------------
-- File History:
--
-- $Log: snespad_pad.vhd,v $
-- Revision 1.2  2005/09/15 17:28:02  arniml
-- remove obsolete variable
--
-- Revision 1.1  2004/10/05 17:01:27  arniml
-- initial check-in
--
-------------------------------------------------------------------------------
