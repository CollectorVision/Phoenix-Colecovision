-------------------------------------------------------------------------------
--
-- SNESpad controller core
--
-- Copyright (c) 2004, Arnim Laeuger (arniml@opencores.org)
--
-- $Id: snespad_pack-p.vhd,v 1.1 2004/10/05 17:01:27 arniml Exp $
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

package snespad_pack is

  constant num_buttons_c : natural := 32;
  subtype  buttons_t is std_logic_vector(num_buttons_c-1 downto 0);
  subtype  num_buttons_read_t is natural range 0 to num_buttons_c-1;

  function button_active_f(state : in std_logic; ref : in natural) return std_logic;
  function button_reset_f(ref : in natural) return std_logic;
  function "=" (a : std_logic; b : integer) return boolean;

  -----------------------------------------------------------------------------
  -- The button positions inside the SNES packet
  -----------------------------------------------------------------------------
  constant but_pos_b_c     : natural := 31;
  constant but_pos_y_c     : natural := 30;
  constant but_pos_sel_c   : natural := 29;
  constant but_pos_start_c : natural := 28;
  constant but_pos_up_c    : natural := 27;
  constant but_pos_down_c  : natural := 26;
  constant but_pos_left_c  : natural := 25;
  constant but_pos_right_c : natural := 24;
  constant but_pos_a_c     : natural := 23;
  constant but_pos_x_c     : natural := 22;
  constant but_pos_tl_c    : natural := 21;
  constant but_pos_tr_c    : natural := 20;
  constant but_pos_bit3_c  : natural := 19;
  constant but_pos_bit2_c  : natural := 18;
  constant but_pos_bit1_c  : natural := 17;
  constant but_pos_bit0_c  : natural := 16;
  constant but_pos_0_c     : natural := 15;
  constant but_pos_1_c     : natural := 14;
  constant but_pos_2_c     : natural := 13;
  constant but_pos_3_c     : natural := 12;
  constant but_pos_4_c     : natural := 11;
  constant but_pos_5_c     : natural := 10;
  constant but_pos_6_c     : natural := 9;
  constant but_pos_7_c     : natural := 8;
  constant but_pos_8_c     : natural := 7;
  constant but_pos_9_c     : natural := 6;
  constant but_pos_star_c  : natural := 5;
  constant but_pos_num_c   : natural := 4;
  constant but_pos_dot_c   : natural := 3;
  constant but_pos_clear_c : natural := 2;
  constant but_pos_unk_c   : natural := 1;
  constant but_pos_equal_c : natural := 0;

end snespad_pack;


package body snespad_pack is

  function button_active_f(state : in std_logic; ref : in natural) return std_logic is
    variable result_v : std_logic;
  begin
    if ref = 0 then
      result_v := state;
    else
      result_v := not state;
    end if;

    return result_v;
  end button_active_f;

  function button_reset_f(ref : in natural) return std_logic is
    variable result_v : std_logic;
  begin
    if ref = 0 then
      result_v := '1';
    else
      result_v := '0';
    end if;

    return result_v;
  end button_reset_f;

  function "=" (a : std_logic; b : integer) return boolean is
    variable result_v : boolean;
  begin
    result_v := false;

    case a is
      when '0' =>
        if b = 0 then
          result_v := true;
        end if;

      when '1' =>
        if b = 1 then
          result_v := true;
        end if;

      when others =>
        null;

    end case;

    return result_v;
  end;


end snespad_pack;
