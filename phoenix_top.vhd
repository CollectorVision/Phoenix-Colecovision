-------------------------------------------------------------------------------
--
-- ColecoFPGA project
--
-- Copyright (c) 2016, Fabio Belavenuto (belavenuto@gmail.com)
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
-------------------------------------------------------------------------------


--
-- Retaining the above copyright for attribution.
--
-- April 2019: Heavily modified and adapted for the Phoenix system.
--             Matthew Hagerty
--


library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;

entity phoenix_top is
   port (
      -- Clocks
      clock_50_i        : in     std_logic;

      -- Buttons
      btn_reset_n_i     : in     std_logic;  -- Active low, pull-up with RC time of ~10ms

      -- External IO port
--      exp_io1_net       : inout  std_logic_vector(12 downto 2) := (others => 'Z');
--      exp_io2_net       : inout  std_logic_vector(24 downto 15):= (others => 'Z');

      -- SRAM
      sram_addr_o       : out    std_logic_vector(18 downto 0) := (others => '0');
      sram_data_io      : inout  std_logic_vector(7 downto 0)  := (others => 'Z');
      sram_ce_n_o       : out    std_logic := '1';
      sram_oe_n_o       : out    std_logic := '1';
      sram_we_n_o       : out    std_logic := '1';

      -- PS2
      ps2_clk_io        : inout  std_logic := 'Z';
      ps2_data_io       : inout  std_logic := 'Z';

      -- SD Card
      sd_cs_n_o         : out    std_logic := '1';
      sd_sclk_o         : out    std_logic := '0';
      sd_mosi_o         : out    std_logic := 'Z';
      sd_miso_i         : in     std_logic;
      sd_cd_n_i         : in     std_logic;           -- Card detect, active low, ext pullup

      -- Flash
      flash_cs_n_o      : out    std_logic := '1';
      flash_sclk_o      : out    std_logic := '0';
      flash_mosi_o      : out    std_logic := '0';
      flash_miso_i      : in     std_logic;
      flash_wp_o        : out    std_logic := '0';
      flash_hold_o      : out    std_logic := '1';

      -- Joystick
      joy_p5_o          : out    std_logic;
      joy_p8_o          : out    std_logic;
      joy1_p1_i         : in     std_logic;
      joy1_p2_i         : in     std_logic;
      joy1_p3_i         : in     std_logic;
      joy1_p4_i         : in     std_logic;
      joy1_p6_i         : in     std_logic;
      joy1_p7_i         : in     std_logic;
      joy1_p9_i         : in     std_logic;
      joy2_p1_i         : in     std_logic;
      joy2_p2_i         : in     std_logic;
      joy2_p3_i         : in     std_logic;
      joy2_p4_i         : in     std_logic;
      joy2_p6_i         : in     std_logic;
      joy2_p7_i         : in     std_logic;
      joy2_p9_i         : in     std_logic;

      -- SNES controller
      snesjoy_clock_o   : out    std_logic := '1';
      snesjoy_latch_o   : out    std_logic := '1';
      snesjoy_data_i    : in     std_logic;

      -- HDMI
      hdmi_p_o          : out    std_logic_vector(3 downto 0);
      hdmi_n_o          : out    std_logic_vector(3 downto 0);

      -- Cartridge
      cart_addr_o       : out    std_logic_vector(14 downto 0) := (others => '0');
      cart_data_io      : inout  std_logic_vector( 7 downto 0);
      cart_dir_o        : out    std_logic := '0'; --Rev6 PCB
      cart_oe_n_o       : out    std_logic := '1';
      cart_en_80_n_o    : out    std_logic := '1';
      cart_en_A0_n_o    : out    std_logic := '1';
      cart_en_C0_n_o    : out    std_logic := '1';
      cart_en_E0_n_o    : out    std_logic := '1';

      -- LED and RGB-LED
      led_o             : out    std_logic := '0';
      rgb_led_o         : out    std_logic_vector(2 downto 0) := (others => '1')
   );
end entity;

use work.cv_keys_pack.all;

architecture behavior of phoenix_top is

   -- Clocks and enables
   signal clock_master_s   : std_logic;
   signal clock_mem_s      : std_logic;
   signal clock_vdp_en_s   : std_logic;
   signal clock_5m_en_s    : std_logic;
   signal clock_3m_en_s    : std_logic;
   signal clock_wsg_en_s   : std_logic;
   signal clock_hdmi_s     : std_logic;
   signal clock_hdmi_n_s   : std_logic;
   signal clk_25m0_s       : std_logic;
   signal clk_100m0_s      : std_logic;

   -- Resets
   signal rst_cnt_r        : unsigned(7 downto 0) := (others => '1');
   signal rst_cnt_x        : unsigned(7 downto 0);
   signal rst_zero_s       : std_logic;
   signal rst_trig_s       : std_logic;
   signal rst_en_s         : std_logic;
   signal por_n_r          : std_logic := '0';
   signal reset_r          : std_logic := '1';
   signal ps2_reset_s      : std_logic;
   signal core_reload_s    : std_logic;

   -- Internal BIOS
   signal bios_addr_s      : std_logic_vector(12 downto 0);    -- 8K
   signal d_from_bios_s    : std_logic_vector( 7 downto 0);
   signal bios_ce_s        : std_logic;
   signal bios_we_s        : std_logic;

   signal d_to_cv_s        : std_logic_vector( 7 downto 0);

   -- RAM memory
   signal ram_addr_s       : std_logic_vector(16 downto 0);    -- 128K
   signal d_from_ram_s     : std_logic_vector( 7 downto 0);
   signal d_to_ram_s       : std_logic_vector( 7 downto 0);
   signal ram_ce_s         : std_logic;
   signal ram_oe_s         : std_logic;
   signal ram_we_s         : std_logic;

   -- VRAM memory
   signal vram_addr_s      : std_logic_vector(13 downto 0);    -- 16K
   signal vram_do_s        : std_logic_vector( 7 downto 0);
   signal vram_di_s        : std_logic_vector( 7 downto 0);
   signal vram_ce_s        : std_logic;
   signal vram_oe_s        : std_logic;
   signal vram_we_s        : std_logic;

   -- Audio
   signal audio_signed_s   : signed(7 downto 0);
   signal audio_s          : std_logic_vector( 7 downto 0);
   signal audio_dac_s      : std_logic;

   -- Video
   signal vga_hsync_n_s    : std_logic;
   signal vga_vsync_n_s    : std_logic;
   signal vga_blank_s      : std_logic;
   signal vga_r_s          : std_logic_vector( 3 downto 0);
   signal vga_g_s          : std_logic_vector( 3 downto 0);
   signal vga_b_s          : std_logic_vector( 3 downto 0);
   signal sound_hdmi_s     : std_logic_vector(15 downto 0);
   signal tdms_r_s         : std_logic_vector( 9 downto 0);
   signal tdms_g_s         : std_logic_vector( 9 downto 0);
   signal tdms_b_s         : std_logic_vector( 9 downto 0);
   signal tdms_p_s         : std_logic_vector( 3 downto 0);
   signal tdms_n_s         : std_logic_vector( 3 downto 0);

   -- Keyboard
   signal ps2_keys_s       : std_logic_vector(15 downto 0);
   signal ps2_joy_s        : std_logic_vector(15 downto 0);
   signal homekey_s        : std_logic;

   -- Controller
   signal ctrl_p1_s        : std_logic_vector( 2 downto 1);
   signal ctrl_p2_s        : std_logic_vector( 2 downto 1);
   signal ctrl_p3_s        : std_logic_vector( 2 downto 1);
   signal ctrl_p4_s        : std_logic_vector( 2 downto 1);
   signal ctrl_p5_s        : std_logic;
   signal ctrl_p6_s        : std_logic_vector( 2 downto 1);
   signal ctrl_p7_s        : std_logic_vector( 2 downto 1);
   signal ctrl_p8_s        : std_logic;
   signal ctrl_p9_s        : std_logic_vector( 2 downto 1);

   -- SNES
   signal but_a_s          : std_logic_vector( 0 downto 0);
   signal but_b_s          : std_logic_vector( 0 downto 0);
   signal but_x_s          : std_logic_vector( 0 downto 0);
   signal but_y_s          : std_logic_vector( 0 downto 0);
   signal but_start_s      : std_logic_vector( 0 downto 0);
   signal but_sel_s        : std_logic_vector( 0 downto 0);
   signal but_tl_s         : std_logic_vector( 0 downto 0);
   signal but_tr_s         : std_logic_vector( 0 downto 0);
   signal but_up_s         : std_logic_vector( 0 downto 0);
   signal but_down_s       : std_logic_vector( 0 downto 0);
   signal but_left_s       : std_logic_vector( 0 downto 0);
   signal but_right_s      : std_logic_vector( 0 downto 0);
   signal but_0_s          : std_logic_vector( 0 downto 0);
   signal but_1_s          : std_logic_vector( 0 downto 0);
   signal but_2_s          : std_logic_vector( 0 downto 0);
   signal but_3_s          : std_logic_vector( 0 downto 0);
   signal but_4_s          : std_logic_vector( 0 downto 0);
   signal but_5_s          : std_logic_vector( 0 downto 0);
   signal but_6_s          : std_logic_vector( 0 downto 0);
   signal but_7_s          : std_logic_vector( 0 downto 0);
   signal but_8_s          : std_logic_vector( 0 downto 0);
   signal but_9_s          : std_logic_vector( 0 downto 0);
   signal but_star_s       : std_logic_vector( 0 downto 0);
   signal but_num_s        : std_logic_vector( 0 downto 0);
   signal but_dot_s        : std_logic_vector( 0 downto 0);
   signal but_clear_s      : std_logic_vector( 0 downto 0);
   signal but_equal_s      : std_logic_vector( 0 downto 0);
   
   -- CV core debug LED.
   signal cv_led_s         : std_logic;
	
	signal wait_n_s			: std_logic;

begin

   --
   -- PLL for master and video clocks.
   --
   pll_1: entity work.pll1
   port map (
      clk_50m0_i     => clock_50_i,       --  50.000
      clk_21m8_o     => clock_master_s,   --  21.47727 (21.739 PLL) MHz (6x NTSC)
      clk_43m5_o     => clock_mem_s,      --  42.95454 (43.478 PLL) MHz
      clk_25m0_o     => clk_25m0_s,       --  25.20000 (25.000 PLL) MHz
      clk_100m0_o    => clk_100m0_s,      -- 100.0000 (100.000 PLL) MHz
      clk_125m0_o    => clock_hdmi_s,     -- 126.0000 (125.000 PLL) MHz
      clk_125m0_n_o  => clock_hdmi_n_s    -- 180o
   );


   --
   -- Clock enables.
   --
   clks: entity work.clocks
   port map (
      clock_i        => clock_master_s,
      clock_3m_en_o  => clock_3m_en_s,
      clock_wsg_en_o => clock_wsg_en_s
   );


   --
   -- The System Reset has multiple sources:
   --
   --  1. Power-On Reset (PoR)
   --  2. Push-Button Reset, does not PoR signal
   --  3. Soft Reset via PS2 keyboard
   --
   -- Some devices do not get reset with the button, like the VDP.
   -- This is accurate to the real ColecoVision.
   --

   -- Soft reset trigger can be the PS2 keyboard ESC key or
   -- the reset button input.
   rst_trig_s <= ps2_reset_s or (not btn_reset_n_i);

   rst_zero_s <= '1' when rst_cnt_r = 0 else '0';

   rst_en_s <= rst_trig_s or (not rst_zero_s);

   rst_cnt_x <= rst_cnt_r - 1;

   process(clock_master_s)
   begin if rising_edge(clock_master_s) then
      if rst_en_s = '1' then
         rst_cnt_r <= rst_cnt_x;
      end if;

      -- PoR is a one-time event at power-on (hence the name).
      if por_n_r = '0' then
         por_n_r <= rst_zero_s;
      end if;

      -- Soft reset any time the reset counter is not zero.
      reset_r <= not rst_zero_s;

   end if;
   end process;


   --
   -- General purpose LED and RGB LED.
   --

   -- LED, as wired, do not set FPGA output high!  That would cause
   -- a path from ground, through the LED, directly to 3.3V.  The
   -- only safe signals are '0' and 'Z' (tri-state floating).
   --
   -- When the FPGA output is 'Z', the LED current will be through the
   -- resistor to 3.3V and it will turn on.
   --
   -- When the FPGA output is '0', the ground is applied to both sides
   -- of the LED and it will be off, but the FPGA output will sink 10mA
   -- through the resistor to 3.3V.
   --
   -- GND---|<|----+---/\/\/---3.3V
   --       led    |    330
   --        __    |
   -- GND---o  o---+
   --     FPGA out, '0' or 'Z' only! Never output a '1'.

   -- Indicate reset with the LED.
   led_o <= 'Z' when reset_r = '1' or cv_led_s = '1' else '0';

   -- RGB-LED, set to yellow
   rgb_led_o <= "001";


   --
   -- ColecoVision Core
   --
   coleco : entity work.colecovision
   generic map (
      mach_id_g         => 8                 -- Phoenix board machine ID for port read.
   )
   port map (
      clock_i           => clock_master_s,   --  21.47727 (21.739 PLL) MHz
      clock_mem_i       => clock_mem_s,      --  42.95454 (43.478 PLL) MHz
      clk_100m0_i       => clk_100m0_s,
      clk_25m0_i        => clk_25m0_s,
      clk_en_3m58_i     => clock_3m_en_s,
      clk_en_1m79_i     => clock_wsg_en_s,
      reset_i           => reset_r,          -- Active high
      por_n_i           => por_n_r,          -- Active low

      -- Physical controller interface
      ctrl_p1_i         => ctrl_p1_s,
      ctrl_p2_i         => ctrl_p2_s,
      ctrl_p3_i         => ctrl_p3_s,
      ctrl_p4_i         => ctrl_p4_s,
      ctrl_p5_o         => ctrl_p5_s,
      ctrl_p6_i         => ctrl_p6_s,
      ctrl_p7_i         => ctrl_p7_s,
      ctrl_p8_o         => ctrl_p8_s,
      ctrl_p9_i         => ctrl_p9_s,

--      -- CPU RAM Interface
--      ram_addr_o        => ram_addr_s,
--      ram_ce_o          => ram_ce_s,
--      ram_we_o          => ram_we_s,
--      ram_oe_o          => ram_oe_s,
--      ram_data_i        => d_to_cv_s,
--      ram_data_o        => d_to_ram_s,

--      -- Video RAM Interface
--      vram_addr_o       => vram_addr_s,
--      vram_ce_o         => vram_ce_s,
--      vram_oe_o         => vram_oe_s,
--      vram_we_o         => vram_we_s,
--      vram_data_i       => vram_do_s,
--      vram_data_o       => vram_di_s,

      -- Physical cartridge interface
      cart_addr_o       => cart_addr_o,
      cart_dir_o        => cart_dir_o,
      cart_data_io      => cart_data_io,
      cart_oe_n_o       => cart_oe_n_o,
      cart_en_80_n_o    => cart_en_80_n_o,
      cart_en_a0_n_o    => cart_en_A0_n_o,
      cart_en_c0_n_o    => cart_en_C0_n_o,
      cart_en_e0_n_o    => cart_en_E0_n_o,

      -- Audio output
      audio_o           => open,
      audio_signed_o    => audio_signed_s,

      -- VGA output
      blank_o           => vga_blank_s,
      hsync_n_o         => vga_hsync_n_s,
      vsync_n_o         => vga_vsync_n_s,
      red_o             => vga_r_s,
      grn_o             => vga_g_s,
      blu_o             => vga_b_s,

      -- External SRAM
      sram_addr_o       => sram_addr_o,
      sram_data_io      => sram_data_io,
      sram_ce_n_o       => sram_ce_n_o,
      sram_oe_n_o       => sram_oe_n_o,
      sram_we_n_o       => sram_we_n_o,

      -- SDcard SPI
      sd_miso_i         => sd_miso_i,
      sd_mosi_o         => sd_mosi_o,
      sd_sclk_o         => sd_sclk_o,
      sd_cs_n_o         => sd_cs_n_o,
      sd_cd_n_i         => sd_cd_n_i,     -- Card detect, active low.
      
      led_o             => cv_led_s,
	  wait_n_i			=> wait_n_s
   );


   -- PS/2 keyboard interface
   ps2if_inst : entity work.colecoKeyboard
   port map (
      clk            => clock_master_s,
      reset          => reset_r,          -- Active high
      -- inputs from PS/2 port
      ps2_clk        => ps2_clk_io,
      ps2_data       => ps2_data_io,
      -- user outputs
      keys           => ps2_keys_s,
      joy            => ps2_joy_s,
      core_reload_o  => core_reload_s,
      home_o         => homekey_s
   );


   --
   -- SNES Gamepad (TODO: implement NTT)
   --
   snespads_b : entity work.snespad
   generic map (
      num_pads_g        => 1,
      reset_level_g     => 0,
      button_level_g    => 0,
      clocks_per_6us_g  => 128               -- 6us = 128 ciclos de 21.477MHz
   )
   port map (
      clk_i             => clock_master_s,
      reset_i           => por_n_r,          -- Active low
      pad_clk_o         => snesjoy_clock_o,
      pad_latch_o       => snesjoy_latch_o,
      pad_data_i(0)     => snesjoy_data_i,
      but_a_o           => but_a_s,
      but_b_o           => but_b_s,
      but_x_o           => but_x_s,
      but_y_o           => but_y_s,
      but_start_o       => but_start_s,
      but_sel_o         => but_sel_s,
      but_tl_o          => but_tl_s,
      but_tr_o          => but_tr_s,
      but_up_o          => but_up_s,
      but_down_o        => but_down_s,
      but_left_o        => but_left_s,
      but_right_o       => but_right_s,
      but_0_o           => but_0_s,
      but_1_o           => but_1_s,
      but_2_o           => but_2_s,
      but_3_o           => but_3_s,
      but_4_o           => but_4_s,
      but_5_o           => but_5_s,
      but_6_o           => but_6_s,
      but_7_o           => but_7_s,
      but_8_o           => but_8_s,
      but_9_o           => but_9_s,
      but_star_o        => but_star_s,
      but_num_o         => but_num_s,
      but_dot_o         => but_dot_s,
      but_clear_o       => but_clear_s,
      but_equal_o       => but_equal_s
   );


   --
   -- Mix the NES GamePad and PS2 keyboard signals to CV Controller 1.
   -- The GamePad and keyboard do not provide quadrature signals, those
   -- inputs are wired directly below this process.
   --
   pad_ctrl: process (
      ctrl_p5_s, ctrl_p8_s, ps2_keys_s, ps2_joy_s,
      joy1_p1_i, joy1_p2_i, joy1_p3_i, joy1_p4_i, joy1_p6_i,
      but_a_s, but_b_s, but_up_s, but_down_s, but_left_s, but_right_s,
      but_x_s, but_y_s, but_sel_s, but_start_s, but_tl_s, but_tr_s, but_0_s,
      but_1_s, but_2_s, but_3_s, but_4_s, but_5_s, but_6_s, but_7_s, but_8_s,
      but_9_s, but_star_s, but_num_s, but_dot_s, but_clear_s, but_equal_s, wait_n_s
   )
      variable key_v   : natural range cv_keys_t'range;
      variable fire1_v : std_logic;
      variable fire2_v : std_logic;
   begin

      -- Joy 1
      if ctrl_p5_s = '0' and ctrl_p8_s = '1' then
         -- keys and right button enabled
         -- keys not fully implemented

         key_v := cv_key_none_c;

         -- PS/2 Keyboard
         if ps2_keys_s(13) = '1' then
            -- KEY 1
            key_v := cv_key_1_c;
         elsif ps2_keys_s(7) = '1' then
            -- KEY 2
            key_v := cv_key_2_c;
         elsif ps2_keys_s(12) = '1' then
            -- KEY 3
            key_v := cv_key_3_c;
         elsif ps2_keys_s(2) = '1' then
            -- KEY 4
            key_v := cv_key_4_c;
         elsif ps2_keys_s(3) = '1' then
            -- KEY 5
            key_v := cv_key_5_c;
         elsif ps2_keys_s(14) = '1' then
            -- KEY 6
            key_v := cv_key_6_c;
         elsif ps2_keys_s(5) = '1' then
            -- KEY 7
            key_v := cv_key_7_c;
         elsif ps2_keys_s(1) = '1' then
            -- KEY 8
            key_v := cv_key_8_c;
         elsif ps2_keys_s(11) = '1' then
            -- KEY 9
            key_v := cv_key_9_c;
         elsif ps2_keys_s(10) = '1' then
            -- KEY 0
            key_v := cv_key_0_c;
         elsif ps2_keys_s(6) = '1' then
            -- KEY *
            key_v := cv_key_asterisk_c;
         elsif ps2_keys_s(9) = '1' then
            -- KEY #
            key_v := cv_key_number_c;
         end if;

         -- SNES joypad 
         if but_tl_s(0) = '0' then           -- TL button pressed, numbers 1 to 6
            if    but_y_s(0) = '0' then
               -- KEY 1
               key_v := cv_key_1_c;
            elsif but_x_s(0) = '0' then
               -- KEY 2
               key_v := cv_key_2_c;
            elsif but_b_s(0) = '0' then
               -- KEY 3
               key_v := cv_key_3_c;
            elsif but_a_s(0) = '0' then
               -- KEY 4
               key_v := cv_key_4_c;
            elsif but_sel_s(0) = '0' then
               -- KEY 5
               key_v := cv_key_5_c;
            elsif but_start_s(0) = '0' then
               -- KEY 6
               key_v := cv_key_6_c;
            end if;
         elsif but_tr_s(0) = '0' then        -- TR button pressed, 7 at 0, * and #
            if    but_y_s(0) = '0' then
               -- KEY 7
               key_v := cv_key_7_c;
            elsif but_x_s(0) = '0' then
               -- KEY 8
               key_v := cv_key_8_c;
            elsif but_b_s(0) = '0' then
               -- KEY 9
               key_v := cv_key_9_c;
            elsif but_a_s(0) = '0' then
               -- KEY 0
               key_v := cv_key_0_c;
            elsif but_sel_s(0) = '0' then
               -- KEY *
               key_v := cv_key_asterisk_c;
            elsif but_start_s(0) = '0' then
               -- KEY #
               key_v := cv_key_number_c;
            end if;
         end if;
         -- NTT data
         if    but_1_s(0) = '0' then
            -- KEY 1
            key_v := cv_key_1_c;
         elsif but_2_s(0) = '0' then
            -- KEY 2
            key_v := cv_key_2_c;
         elsif but_3_s(0) = '0' then
            -- KEY 3
            key_v := cv_key_3_c;
         elsif but_4_s(0) = '0' then
            -- KEY 4
            key_v := cv_key_4_c;
         elsif but_5_s(0) = '0' then
            -- KEY 5
            key_v := cv_key_5_c;
         elsif but_6_s(0) = '0' then
            -- KEY 6
            key_v := cv_key_6_c;
         elsif but_7_s(0) = '0' then
            -- KEY 7
            key_v := cv_key_7_c;
         elsif but_8_s(0) = '0' then
            -- KEY 8
            key_v := cv_key_8_c;
         elsif but_9_s(0) = '0' then
            -- KEY 9
            key_v := cv_key_9_c;
         elsif but_0_s(0) = '0' then
            -- KEY 0
            key_v := cv_key_0_c;
         elsif but_star_s(0) = '0' then
            -- KEY star
            key_v := cv_key_asterisk_c;
         elsif but_num_s(0) = '0' then
            -- KEY #
            key_v := cv_key_number_c;
         end if;

         if but_tl_s(0) = '1' and but_tr_s(0) = '1' then
            if but_y_s(0) = '0' then
               --KEY F3
               key_v := cv_key_f3_c;
            elsif but_x_s(0) = '0' then
               --KEY F4
               key_v := cv_key_f4_c;
            end if;
         end if;

         ctrl_p1_s(1) <= cv_keys_c(key_v)(1) and joy1_p1_i;
         ctrl_p2_s(1) <= cv_keys_c(key_v)(2) and joy1_p2_i;
         ctrl_p3_s(1) <= cv_keys_c(key_v)(3) and joy1_p3_i;
         ctrl_p4_s(1) <= cv_keys_c(key_v)(4) and joy1_p4_i;

         if but_tl_s(0) = '1' and but_tr_s(0) = '1' then    -- fire 2 only if the TL and TR buttons are not pressed
            fire2_v := but_a_s(0) and joy1_p6_i;
         else
            fire2_v := '1';
         end if;

         ctrl_p6_s(1) <= not ps2_keys_s(0) and fire2_v and joy1_p6_i; -- button right (fire 2)

      elsif ctrl_p5_s = '1' and ctrl_p8_s = '0' then
         -- joystick and left button enabled
         ctrl_p1_s(1) <= not ps2_joy_s(0) and but_up_s(0) and but_dot_s(0) and joy1_p1_i;    -- up
         ctrl_p2_s(1) <= not ps2_joy_s(1) and but_down_s(0) and joy1_p2_i; -- down
         ctrl_p3_s(1) <= not ps2_joy_s(2) and but_left_s(0) and joy1_p3_i; -- left
         ctrl_p4_s(1) <= not ps2_joy_s(3) and but_right_s(0) and joy1_p4_i;   -- right

         if but_tl_s(0) = '1' and but_tr_s(0) = '1' then    -- fire 1 only if the TL and TR buttons are not pressed
            fire1_v := but_b_s(0) and joy1_p6_i;
         else
            fire1_v := '1';
         end if;

         ctrl_p6_s(1) <= not ps2_joy_s(4) and fire1_v;   -- button left (fire 1)
      else
         -- nothing active
         ctrl_p1_s(1)   <= joy1_p1_i;
         ctrl_p2_s(1)   <= joy1_p2_i;
         ctrl_p3_s(1)   <= joy1_p3_i;
         ctrl_p4_s(1)   <= joy1_p4_i;
         ctrl_p6_s(1)   <= joy1_p6_i;
      end if;

   end process pad_ctrl;

   -- PS2 system soft reset signal via the ESC key.
   ps2_reset_s <= ps2_keys_s(8);

   -- Controller 2, straight input signals.
   ctrl_p1_s(2) <= joy2_p1_i;
   ctrl_p2_s(2) <= joy2_p2_i;
   ctrl_p3_s(2) <= joy2_p3_i;
   ctrl_p4_s(2) <= joy2_p4_i;
   ctrl_p6_s(2) <= joy2_p6_i;

   -- Quadrature inputs for both controllers.
   ctrl_p7_s <= joy2_p7_i & joy1_p7_i;
   ctrl_p9_s <= joy2_p9_i & joy1_p9_i;
   --ctrl_p7_s <= "11"; --disable encoder lines
   --ctrl_p9_s <= "11"; --disable encoder lines
   
   -- Output stick vs keypad selection.
   joy_p5_o <= ctrl_p5_s;
   joy_p8_o <= ctrl_p8_s;


   --
   -- HDMI
   --

   audio_s <= std_logic_vector(unsigned(signed(audio_signed_s)+128));
   sound_hdmi_s <= "0" & audio_s & "0000000";

   hdmi: entity work.hdmi
   generic map (
      FREQ  => 25000000,   -- pixel clock frequency
      FS    => 48000,      -- audio sample rate - should be 32000, 41000 or 48000 = 48KHz
      CTS   => 25000,      -- CTS = Freq(pixclk) * N / (128 * Fs)
      N     => 6144        -- N = 128 * Fs /1000,  128 * Fs /1500 <= N <= 128 * Fs /300 (Check HDMI spec 7.2 for details)
   )
   port map (
      I_CLK_PIXEL    => clk_25m0_s,
      I_R            => vga_r_s & vga_r_s,
      I_G            => vga_g_s & vga_g_s,
      I_B            => vga_b_s & vga_b_s,
      I_BLANK        => vga_blank_s,
      I_HSYNC        => vga_hsync_n_s,
      I_VSYNC        => vga_vsync_n_s,
      -- PCM audio
      I_AUDIO_ENABLE => '1',
      I_AUDIO_PCM_L  => sound_hdmi_s,
      I_AUDIO_PCM_R  => sound_hdmi_s,
      -- TMDS parallel pixel synchronous outputs (serialize LSB first)
      O_RED          => tdms_r_s,
      O_GREEN        => tdms_g_s,
      O_BLUE         => tdms_b_s
   );

   hdmio: entity work.hdmi_out_xilinx
   port map (
      clock_pixel_i     => clk_25m0_s,
      clock_tdms_i      => clock_hdmi_s,
      clock_tdms_n_i    => clock_hdmi_n_s,
      red_i             => tdms_r_s,
      green_i           => tdms_g_s,
      blue_i            => tdms_b_s,
      tmds_out_p        => hdmi_p_o,
      tmds_out_n        => hdmi_n_o
   );

end architecture;
