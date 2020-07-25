--
-- Phoenix ColecoVision Core
-- Released under the 3-Clause BSD License:
--
-- Copyright 2019 - 2020
-- Matthew Hagerty, Brian Burney
--
-- This core started as a mix of the ColecoFPGA core and a CV core designed and
-- written by Matthew.  By the time of the first release, this CV core has been
-- completely rewritten several times over, and nothing of either original core
-- remains. However, both systems provided information and a starting point for
-- this implementation, and credit is always due.
--
-- Attribution for the ColecoFPGA project:
--
-------------------------------------------------------------------------------
--
-- ColecoFPGA project
--
-- Copyright (c) 2006, Arnim Laeuger (arnim.laeuger@gmx.net)
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

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity colecovision is
generic
   ( mach_id_g             : integer := 0
);
port
   ( clk_25m0_i            : in  std_logic
   ; clk_100m0_i           : in  std_logic
   ; clk_3m58_en_i         : in  std_logic
   ; reset_i               : in  std_logic -- Soft Reset, active high
   ; por_n_i               : in  std_logic -- Power-on Reset, active low

   -- Physical controller interface
   ; ctrl_p1_i             : in  std_logic_vector( 2 downto 1)
   ; ctrl_p2_i             : in  std_logic_vector( 2 downto 1)
   ; ctrl_p3_i             : in  std_logic_vector( 2 downto 1)
   ; ctrl_p4_i             : in  std_logic_vector( 2 downto 1)
   ; ctrl_p5_o             : out std_logic
   ; ctrl_p6_i             : in  std_logic_vector( 2 downto 1)
   ; ctrl_p7_i             : in  std_logic_vector( 2 downto 1)
   ; ctrl_p8_o             : out std_logic
   ; ctrl_p9_i             : in  std_logic_vector( 2 downto 1)

   -- Physical cartridge interface
   ; cart_addr_o           : out std_logic_vector(14 downto 0) -- 32K
   ; cart_dir_o            : out std_logic
   ; cart_data_io          : inout std_logic_vector( 7 downto 0)
   ; cart_oe_n_o           : out std_logic
   ; cart_en_80_n_o        : out std_logic
   ; cart_en_a0_n_o        : out std_logic
   ; cart_en_c0_n_o        : out std_logic
   ; cart_en_e0_n_o        : out std_logic

   -- Audio output
   ; pcm16_o               : out std_logic_vector(15 downto 0)

   -- VGA output
   ; blank_o               : out std_logic
   ; hsync_n_o             : out std_logic
   ; vsync_n_o             : out std_logic
   ; red_o                 : out std_logic_vector( 3 downto 0)
   ; grn_o                 : out std_logic_vector( 3 downto 0)
   ; blu_o                 : out std_logic_vector( 3 downto 0)

   -- External SRAM, 512Kx8, 10ns
   ; sram_addr_o           : out    std_logic_vector(18 downto 0)
   ; sram_data_io          : inout  std_logic_vector(7 downto 0)
   ; sram_ce_n_o           : out    std_logic
   ; sram_oe_n_o           : out    std_logic
   ; sram_we_n_o           : out    std_logic

   -- SD-card SPI interface
   ; sd_miso_i             : in  std_logic
   ; sd_mosi_o             : out std_logic
   ; sd_sclk_o             : out std_logic
   ; sd_cs_n_o             : out std_logic
   ; sd_cd_n_i             : in  std_logic -- Card detect, active low.

   ; led_o                 : out std_logic -- debug LED
);
end entity;

architecture rtl of colecovision is

   signal reset_n_s                       : std_logic;

   -- Z80 CPU
   signal clk_en_cpu_s                    : std_logic;
   signal nmi_n_s                         : std_logic;
   signal int_n_s                         : std_logic;
   signal iorq_n_s                        : std_logic;
   signal m1_n_s                          : std_logic;
   signal m1_n_r                          : std_logic := '1';
   signal m1_wait_n_r                     : std_logic := '1';
   signal rd_n_s                          : std_logic;
   signal wr_n_s                          : std_logic;
   signal mreq_n_s                        : std_logic;
   signal rfsh_n_s                        : std_logic;
   signal cpu_addr_s                      : std_logic_vector(15 downto 0);
   signal d_to_cpu_s                      : std_logic_vector( 7 downto 0);
   signal d_from_cpu_s                    : std_logic_vector( 7 downto 0);

   -- F18A VDP
   signal d_from_vdp_s                    : std_logic_vector( 7 downto 0);
   signal vdp_r_n_s                       : std_logic;
   signal vdp_w_n_s                       : std_logic;
   signal vdp_settings_r						: std_logic_vector( 1 downto 0) := "00";

   -- SN76489 PSG
	signal sn489_ready_s                   : std_logic;
   signal sn489_we_n_s                    : std_logic;
   signal sn76489_pcm14s_s                : unsigned(13 downto 0);

   -- YM-2149 / AY-3-8910 PSG
   signal d_from_wsg_s                    : std_logic_vector(7 downto 0);
   signal wsg_bdir_s                      : std_logic;
   signal wsg_bc_s                        : std_logic;
   signal ym2149_pcm14s_s                 : unsigned(13 downto 0);

   -- Audio Mixing
   signal pcm16_r                         : unsigned (15 downto 0) := (others => '0');
   signal pcm16_x                         : unsigned (15 downto 0);

   -- Internal 1K RAM
   type ram1k_t is array (0 to 1023) of std_logic_vector( 7 downto 0);

   signal cv_ram1k_r                      : ram1k_t;
   signal cv_ram1k_en_s                   : std_logic;
   signal cv_ram1k_addr_s                 : std_logic_vector( 9 downto 0);
   signal d_from_cv_ram1k_r               : std_logic_vector( 7 downto 0);

   -- SGM RAM
   signal sgm_en_r                        : std_logic := '0';
   signal sgm_en_x                        : std_logic;
   signal sgm_8k_en_r                     : std_logic := '0';
   signal sgm_8k_en_x                     : std_logic;

   -- External SRAM
   signal ext_ram_addr_s                  : std_logic_vector(18 downto 0);
   signal ext_ram_bank_r                  : std_logic_vector( 3 downto 0) := "0001";
   signal ext_ram_bank_x                  : std_logic_vector( 3 downto 0);
   signal ext_ram_en_s                    : std_logic := '0';
   signal ext_ram_we_n_s                  : std_logic;
   signal d_from_ext512_s                 : std_logic_vector( 7 downto 0);

   -- Data and IO bus
   signal io_decode_s                     : std_logic_vector( 2 downto 0);
   signal cpu_data_mux_s                  : std_logic_vector( 7 downto 0);
   signal io_data_mux_s                   : std_logic_vector( 7 downto 0);
   signal ex_data_mux_s                   : std_logic_vector( 7 downto 0);

   -- BIOS
   signal d_from_bios_s                   : std_logic_vector( 7 downto 0);
   signal bios_we_s                       : std_logic;

   -- ROM Loader
   signal rom_loader_en_r                 : std_logic := '1'; -- at power-up the loader is active.
   signal rom_loader_en_x                 : std_logic;
   signal d_from_loader_s                 : std_logic_vector( 7 downto 0);
   signal rom_loader_we_n_s               : std_logic := '1';

   -- SD-card support
   signal d_from_sd_s                     : std_logic_vector( 7 downto 0);
   signal sd_cs_n_s                       : std_logic;        -- SD-card selected
   signal sd_wait_n_s                     : std_logic;        -- CPU should wait for SD-card
   signal sd_detect_r                     : std_logic := '0'; -- default no card.
   signal sd_slow_clk_r                   : std_logic := '1'; -- start SPI with slow clock
   signal sd_slow_clk_x                   : std_logic;
   signal sd_spi_ss_n_r                   : std_logic := '1'; -- SD-card SPI device select
   signal sd_spi_ss_n_x                   : std_logic;

   -- Machine ID
   constant machine_id_c                  : std_logic_vector( 7 downto 0) := std_logic_vector(to_unsigned(mach_id_g, 8));

   -- External cartridge
   signal cart_en_80_n_s                  : std_logic;
   signal cart_en_A0_n_s                  : std_logic;
   signal cart_en_C0_n_s                  : std_logic;
   signal cart_en_E0_n_s                  : std_logic;

   -- Controllers
   signal d_from_ctrl_s                   : std_logic_vector( 7 downto 0);
   signal ctrl_en_key_n_s                 : std_logic;
   signal ctrl_en_joy_n_s                 : std_logic;
   signal ctrl_int_n_s                    : std_logic;

   -- Upper memory selection and bank schemes
   signal real_cart_r                     : std_logic := '1'; -- '1' when using a real cartridge
   signal real_cart_x                     : std_logic;
   signal bank_mode_r                     : std_logic_vector( 1 downto 0) := "00";
   signal bank_mode_x                     : std_logic_vector( 1 downto 0);
   signal upmem_mode_r                    : std_logic_vector( 1 downto 0) := "00";
   signal upmem_mode_x                    : std_logic_vector( 1 downto 0);
   signal upmem_we_inhibit_s              : std_logic;

   -- MegaCart banking support
   signal mc_mem_size_r                   : std_logic_vector( 4 downto 0) := "01111"; -- 256K
   signal mc_mem_size_x                   : std_logic_vector( 4 downto 0);
   signal mc_bank_r                       : std_logic_vector( 4 downto 0) := "11111";
   signal mc_bank_x                       : std_logic_vector( 4 downto 0);
   signal mc_addr_r                       : std_logic_vector( 4 downto 0) := "11111";
   signal mc_addr_x                       : std_logic_vector( 4 downto 0);
   signal mc_en_r                         : std_logic := '0';
   signal mc_en_x                         : std_logic;

begin

   -- Debug LED.
   led_o <= rom_loader_en_r;

   -- Active-low version of the soft reset.
   reset_n_s <= not reset_i;

   --
   -- Instruction Fetch Wait State Generator.
   --
   -- Z80-based systems, like the ColecoVision and MSX computers, have a
   -- circuit that inserts a wait-state during the instruction fetch cycle.
   -- It is unclear why this is necessary, but that mystery will have to be
   -- solved later.
   --
   m1_wait: process (clk_25m0_i, reset_n_s, m1_n_s)
   begin
   if rising_edge(clk_25m0_i) then

      m1_n_r <= m1_n_s;

      -- Disable during reset or non-instruction fetch cycles.
      if reset_n_s = '0' or m1_n_s = '1' then
         m1_wait_n_r <= '1';

      -- Edge-detect the instruction-fetch signal and assert the wait.
      elsif m1_n_r = '1' and m1_n_s = '0' then
         m1_wait_n_r <= '0';

      -- Remove the wait on the next machine cycle.
      elsif clk_3m58_en_i = '1' then
         m1_wait_n_r <= '1';
      end if;
   end if;
   end process m1_wait;


   --
   -- Z80 CPU, synchronous top level with clock enable
   --

   -- TODO: Matthew H, March 19, 2019
   -- Why is the wait being created via the clock enable instead of the
   -- real WAIT_n input to the CPU?  Is the T80 core WAIT_n broken?
   --
   -- Modified CPU clock enable based on slow devices or the wait state.
   clk_en_cpu_s <= clk_3m58_en_i and sn489_ready_s and m1_wait_n_r and sd_wait_n_s;

   -- CPU maskable interrupt input, currently only the controllers.
   int_n_s <= ctrl_int_n_s;

   inst_t80 : entity work.t80se
   port map
   ( RESET_n   => reset_n_s    -- in std_logic;
   , CLK_n     => clk_25m0_i   -- in std_logic;
   , CLKEN     => clk_en_cpu_s -- in std_logic;
   , WAIT_n    => '1'          -- in std_logic;
   , INT_n     => int_n_s      -- in std_logic;
   , NMI_n     => nmi_n_s      -- in std_logic;
   , BUSRQ_n   => '1'          -- in std_logic;
   , M1_n      => m1_n_s       -- out std_logic;
   , MREQ_n    => mreq_n_s     -- out std_logic;
   , IORQ_n    => iorq_n_s     -- out std_logic;
   , RD_n      => rd_n_s       -- out std_logic;
   , WR_n      => wr_n_s       -- out std_logic;
   , RFSH_n    => rfsh_n_s     -- out std_logic;
   , HALT_n    => open         -- out std_logic;
   , BUSAK_n   => open         -- out std_logic;
   , A         => cpu_addr_s   -- out std_logic_vector(15 downto 0);
   , DI        => d_to_cpu_s   -- in std_logic_vector(7 downto 0);
   , DO        => d_from_cpu_s -- out std_logic_vector(7 downto 0)
   );


   --
   -- F18A VDP
   --
   -- The soft button reset is sent to the F18A (unlike the real CV) to ensure
   -- a game will not leave enhanced F18A settings enabled which will cause
   -- problems for legacy or non-F18A games.
   --
   inst_f18a : entity work.f18a_core
   port map
   ( clk_100m0_i    => clk_100m0_i  -- must be 100MHz
   , clk_25m0_i     => clk_25m0_i   -- must be an actual 25MHz clock, NOT an enable
   -- 9918A to Host interface
   , reset_n_i      => reset_n_s    -- active low, the F18A gets soft reset
   , mode_i         => cpu_addr_s(0)
   , csw_n_i        => vdp_w_n_s
   , csr_n_i        => vdp_r_n_s
   , int_n_o        => nmi_n_s
   , cd_i           => d_from_cpu_s
   , cd_o           => d_from_vdp_s
   -- Video Output
   , blank_o        => blank_o      -- active high during active video
   , hsync_o        => hsync_n_o
   , vsync_o        => vsync_n_o
   , red_o          => red_o        -- 4-bit red
   , grn_o          => grn_o        -- 4-bit green
   , blu_o          => blu_o        -- 4-bit blue
   -- Feature Selection
   , sprite_max_i   => vdp_settings_r(1) -- default sprite max, '0'=32, '1'=4
   , scanlines_i    => vdp_settings_r(0) -- simulated scan lines, '0'=no, '1'=yes
   -- SPI for GPU access, not enabled in the Phoenix
   , spi_clk_o      => open
   , spi_cs_o       => open
   , spi_mosi_o     => open
   , spi_miso_i     => '0'
   );


   --
   -- SN76489 Programmable Sound Generator
   --
   sn76489_audio_inst : entity work.sn76489_audio
   generic map
   ( MIN_PERIOD_CNT_G => 17 -- Accommodate games that use audible-carrier A.M.
   )
   port map
   ( clk_i        => clk_25m0_i    -- System clock
   , en_clk_psg_i => clk_3m58_en_i -- PSG clock enable
   , ce_n_i       => sn489_we_n_s  -- chip enable, active low
   , wr_n_i       => sn489_we_n_s  -- write enable, active low
   , ready_o      => sn489_ready_s -- low during I/O operations
   , data_i       => d_from_cpu_s
   , ch_a_o       => open
   , ch_b_o       => open
   , ch_c_o       => open
   , noise_o      => open
   , mix_audio_o  => open
   , pcm14s_o     => sn76489_pcm14s_s
   );


   --
   -- YM-2149 (AY-3-8910) Programmable Sound Generator
   --
   ym2149_audio_inst: entity work.ym2149_audio
   port map
   ( clk_i        => clk_25m0_i     -- System clock
   , en_clk_psg_i => clk_3m58_en_i  -- PSG clock enable
   , sel_n_i      => '0'            -- divide the input clock by 2
   , reset_n_i    => reset_n_s      -- active low
   , bc_i         => wsg_bc_s       -- bus control
   , bdir_i       => wsg_bdir_s     -- bus direction
   , data_i       => d_from_cpu_s
   , data_r_o     => d_from_wsg_s   -- registered output data
   , ch_a_o       => open
   , ch_b_o       => open
   , ch_c_o       => open
   , mix_audio_o  => open
   , pcm14s_o     => ym2149_pcm14s_s
   );


   -- Mix the 14-bit signed PCM audio signals into a signed 16-bit PCM sample.
   pcm16_x <= (
      (sn76489_pcm14s_s(13) & sn76489_pcm14s_s) +
      ( ym2149_pcm14s_s(13) &  ym2149_pcm14s_s) ) & "0";

   pcm16_o <= std_logic_vector(pcm16_r);

   -- Register the final audio sample.
   process (clk_25m0_i) begin
   if rising_edge(clk_25m0_i) then
   if clk_3m58_en_i = '1' then
      pcm16_r <= pcm16_x;
   end if;
   end if; end process;


   --
   -- Controller ports
   --
   ctrl_b : entity work.cv_ctrl
   port map
   ( clk_i           => clk_25m0_i
   , clk_3m58_en_i   => clk_3m58_en_i
   , reset_n_i       => reset_n_s
   , ctrl_en_key_n_i => ctrl_en_key_n_s
   , ctrl_en_joy_n_i => ctrl_en_joy_n_s
   , ctrl_p1_i       => ctrl_p1_i
   , ctrl_p2_i       => ctrl_p2_i
   , ctrl_p3_i       => ctrl_p3_i
   , ctrl_p4_i       => ctrl_p4_i
   , ctrl_p5_o       => ctrl_p5_o
   , ctrl_p6_i       => ctrl_p6_i
   , ctrl_p7_i       => ctrl_p7_i
   , ctrl_p8_o       => ctrl_p8_o
   , ctrl_p9_i       => ctrl_p9_i
   , ctrl_sel_i      => cpu_addr_s(1) -- A1=0 read controller 1, A1=1 read controller 2
   , ctrl_d_o        => d_from_ctrl_s
   , ctrl_int_n_o    => ctrl_int_n_s
   );


   --
   -- SD-card SPI interface
   --
   sdcard: entity work.sdcard
   port map
   ( clk_i        => clk_25m0_i
   , reset_n_i    => reset_n_s
   , slow_clk_i   => sd_slow_clk_r -- '1' = use slow SPI clock
   , spi_ss_n_i   => sd_spi_ss_n_r
   , cs_n_i       => sd_cs_n_s
   , wait_n_o     => sd_wait_n_s
   , wr_n_i       => wr_n_s
   , data_i       => d_from_cpu_s
   , data_o       => d_from_sd_s
   -- SD card interface
   , spi_cs_n_o   => sd_cs_n_o
   , spi_sclk_o   => sd_sclk_o
   , spi_mosi_o   => sd_mosi_o
   , spi_miso_i   => sd_miso_i
   );


   --
   -- The loader ROM and BIOS have different addresses at PoR to give the
   -- loader extra room, contain interrupt vectors, and make development
   -- easier.  Once the loader is done it will read port >55 which will
   -- disable the loader and move the 8K BIOS to the original location.
   -- The loader can also overwrite the BIOS with a file from SD.
   --
   -- NOTE: The code that reads port >55 better be in the 1K RAM!
   --
   --                       Loader
   --  Address         Enabled  Disabled
   -- ------------------------------------
   -- >0000 .. >1FFF   Loader   BIOS
   -- >2000 .. >3FFF   Loader   no memory
   -- >4000 .. >5FFF   BIOS     no memory
   --

   --
   -- 24K ROM Loader
   --
   loader: entity work.romloader
   port map
   ( clock_i      => clk_25m0_i
   , we_n_i       => rom_loader_we_n_s
   , addr_i       => cpu_addr_s(14 downto 0)
   , data_i       => d_from_cpu_s
   , data_o       => d_from_loader_s
   );


   --
   -- 8K ROM BIOS
   --
   bios: entity work.rombios
   port map
   ( clock_i      => clk_25m0_i
   , we_i         => bios_we_s
   , addr_i       => cpu_addr_s(12 downto 0)
   , data_i       => d_from_cpu_s
   , data_o       => d_from_bios_s
   );

   -- TODO Maybe allow BIOS to be written to?
   bios_we_s <= '0';


   --
   -- CV Internal 1K RAM
   --
   cv_ram_1k : process (clk_25m0_i, cv_ram1k_en_s, cv_ram1k_addr_s, wr_n_s)
   begin
   if rising_edge(clk_25m0_i) then
      if cv_ram1k_en_s = '1' then
         if wr_n_s = '0' then
            cv_ram1k_r(to_integer(unsigned(cv_ram1k_addr_s))) <= d_from_cpu_s;
         end if;
         d_from_cv_ram1k_r <= cv_ram1k_r(to_integer(unsigned(cv_ram1k_addr_s)));
      end if;
   end if;
   end process;

   cv_ram1k_addr_s <= cpu_addr_s(9 downto 0);


   --
   -- External SRAM, 512Kx8, 10ns
   --
   ext512 : entity work.ext512x8sram
   port map
   ( clk_i        => clk_25m0_i
   -- Z80 CPU
   , cpu_addr_i   => ext_ram_addr_s
   , cpu_en_i     => ext_ram_en_s   -- active high
   , cpu_we_n_i   => ext_ram_we_n_s -- active low
   , cpu_data_i   => d_from_cpu_s
   , cpu_data_o   => d_from_ext512_s
   -- External SRAM Interface
   , sram_addr_o  => sram_addr_o
   , sram_data_io => sram_data_io
   , sram_ce_n_o  => sram_ce_n_o
   , sram_oe_n_o  => sram_oe_n_o
   , sram_we_n_o  => sram_we_n_o
   );


   --
   -- External Cartridge Interface
   --                    74245
   --  Op      FPGA   !OE   DIR
   -- ------------------------------
   -- read      Z      0     0 (B->A) (cart->FPGA)
   -- write    CPU     0     1 (A->B) (FPGA->cart)
   -- !cart     Z      1     X
   --

   -- TODO: Maybe register the cart output so they do not glitch,
   -- since they are driving real external hardware.

   -- The output enable for the 74245 must remain active all the time
   -- so the cart_en_XX_n_s outputs remain valid; The output-enable for
   -- both halves of the 74245 are tied to the same FPGA output.
   -- New carts do not seem to tolerate invalid state on these lines
   -- even when the cartridge is not being addressed.
   cart_oe_n_o    <= '0';
   cart_dir_o     <= rd_n_s;

   -- Only tri-state when reading, otherwise just drive whatever is on
   -- the CPU bus out to the cartridge.  This is what the real CV does.
   cart_data_io   <= (others => 'Z') when rd_n_s = '0' else d_from_cpu_s;
   cart_addr_o    <= cpu_addr_s(14 downto 0);

   -- Individual cartridge range select should always be valid. These are
   -- combinatorial signals and may glitch, however in the real CV they
   -- are also combinatorial (derived from a 74138 which is driven by
   -- the address bus).
   cart_en_80_n_o <= cart_en_80_n_s;
   cart_en_a0_n_o <= cart_en_A0_n_s;
   cart_en_c0_n_o <= cart_en_C0_n_s;
   cart_en_e0_n_o <= cart_en_E0_n_s;


   --
   -- CPU Data-In Mux
   --
   d_to_cpu_s <=
      -- Memory
      cpu_data_mux_s when (mreq_n_s = '0' and rfsh_n_s = '1') else
      -- Original IO ports >80 .. >FF
      io_data_mux_s when cpu_addr_s(7) = '1' else
      -- Extended non-standard IO ports >50 .. >7F
      ex_data_mux_s;


   -- CV Memory Map Decode
   --
   -- A15 A14 A13
   --  8   4   2
   -- -----------
   --  0   0   0   >0000 .. >1FFF (8K)    BIOS, 8K
   --  0   0   1   >2000 .. >3FFF (8K)    EXT_20, no physical memory
   --  0   1   0   >4000 .. >5FFF (8K)    EXT_40, no physical memory
   --  0   1   1   >6000 .. >7FFF (8K)    RAM, 1K, appears at every 1K boundary
   --  1   0   0   >8000 .. >9FFF (8K)    ROM_80
   --  1   0   1   >A000 .. >BFFF (8K)    ROM_A0
   --  1   1   0   >C000 .. >DFFF (8K)    ROM_C0
   --  1   1   1   >E000 .. >FFFF (8K)    ROM_E0
   --
   --
   -- ROM Addressing During Loading
   --
   -- During PoR a 24K ROM loader and 8K RAM is mapped into the bottom 32K of
   -- memory, and the BIOS is not accessible.  When the loader is disabled, the
   -- original BIOS and 1K RAM are visible in their original location.
   --
   --                 Loader: Enabled  Disabled
   --                 ---------------------------
   --  >0000 .. >1FFF (8K)    Loader   BIOS
   --  >2000 .. >3FFF (8K)    Loader   no memory
   --  >4000 .. >5FFF (8K)    Loader   no memory
   --  >6000 .. >63FF (1K)    RAM      RAM
   --  >6400 .. >7FFF (7K)    RAM      mirror of >6000
   --
   -- ADAM Memory Banking
   --
   -- The ADAM computer added a custom memory decode IC that allows for a
   -- flexible selection of RAM or ROM in the upper and lower 32K sections
   -- of the CPU address space.  The Super Game Module (SGM) simply copies
   -- portions of the ADAM's memory map for us on the CV.  The memory
   -- options are selected via I/O port >7F.
   --
   -- Port >7F  CPU Address Space
   --  8 4 2 1  >0000 .. >7FFF
   --  --------------------------------
   --      0 0  SmartWRITER and EOS ROM
   --      0 1  32K Lower Internal RAM
   --      1 0  32K Lower Expansion RAM
   --      1 1  8K BIOS + 24K Lower Internal RAM
   --
   --  8 2 4 1  >8000 .. >FFFF
   --  ---------------------------------
   --  0 0      32K Upper Internal RAM
   --  0 1      32K Expansion ROM
   --  1 0      32K Upper Expansion RAM
   --  1 1      32K Cartridge ROM
   --
   --
   -- SGM Memory
   --
   -- The SGM copies the ADAM Lower Internal RAM option to provide either
   -- 8K BIOS + 24K RAM, or 32K RAM in the lower 32K address range >0000
   -- to >7FFF.
   --
   -- The SGM is enabled via port >53 bit >01=1, and the BIOS+24K or full
   -- 32K option is selected via the ADAM port >7F bit >02.
   --
   -- It is not clear if software on the CV written to use the SGM manages
   -- the 4-bits in port >7F correctly, i.e. as per the ADAM specification.
   -- To support poorly written software, only bit >02 of port >7F will be
   -- used to select between the BIOS or 8K RAM.
   --
   -- Port: >53   >7F   CPU Address Space
   --  bit:   1   2 1   >0000 .. >7FFFF
   --        ---------  ---------------------
   --         0   x x   Original CV BIOS and 1K RAM
   --         1   0 x   32K SGM RAM
   --         1   1 x   CV 8K BIOS + 24K SGM RAM
   --
   -- **Note that the original CV 1K RAM and the SGM RAM are *separate*
   --   memories. Some software might rely on this trait.
   --
   -- The CV has separate memory and I/O decode override inputs on the
   -- expansion port.  This allows expansion modules like the SGM to replace
   -- the system BIOS with RAM.
   --
   --
   -- MegaCart
   --
   -- The MegaCart bank select register is updated any time memory is accessed
   -- in the range >FFC0 to >FFFF.  The LS-address bits (5..0) are used to
   -- specify the bank.  The CV does not provide read or write enables to the
   -- cartridge port, so the enable for updating the bank select register is
   -- any access to the specified memory range.

   memory_decode : process
   ( cpu_addr_s, mreq_n_s, rfsh_n_s, wr_n_s
   , d_from_loader_s, d_from_bios_s, d_from_cv_ram1k_r, cart_data_io
   , d_from_ext512_s, sgm_en_r, sgm_8k_en_r
   , ext_ram_bank_r, rom_loader_en_r, real_cart_r, upmem_mode_r
   , mc_bank_r, mc_addr_r, mc_en_r
   , cart_en_80_n_s, cart_en_A0_n_s, upmem_we_inhibit_s
   ) begin

      cv_ram1k_en_s  <= '0';

      cart_en_80_n_s <= '1';
      cart_en_A0_n_s <= '1';
      cart_en_C0_n_s <= '1';
      cart_en_E0_n_s <= '1';

      -- Default to data from a cartridge.
      cpu_data_mux_s <= cart_data_io;


      -- External 512K SRAM address select priority decode:
      --
      --   1. Select the fixed 32K SGM bank-0 when addressing the lower 32K.
      --   2. If banking, choose address based on the selected bank method.
      --   3. Otherwise use the 32K bank register.

      if cpu_addr_s(15) = '0' then
         -- Address the SGM 32K at the fixed bank-0 in the 512K.
         ext_ram_addr_s <= "0000" & cpu_addr_s(14 downto 0);
      else
         if mc_en_r = '1' then
            -- Banking enabled and MegaCart enabled, check which half of the
            -- upper 32K of memory is being addressed.
            if cart_en_80_n_s = '0' or cart_en_A0_n_s = '0' then
               -- Lower 16K of the upper 32K is *always* the fixed top page.
               ext_ram_addr_s <= "11111" & cpu_addr_s(13 downto 0);
            else
               -- Upper 16K of the upper 32K, use the selected bank.
               ext_ram_addr_s <= mc_addr_r & cpu_addr_s(13 downto 0);
            end if;
         else
            -- Use the 32K bank register.
            ext_ram_addr_s <= ext_ram_bank_r & cpu_addr_s(14 downto 0);
         end if;
      end if;

      ext_ram_en_s      <= '0';
      ext_ram_we_n_s    <= '1';


      -- Required to keep loader ROM from being removed by the synthesizer.
      rom_loader_we_n_s <= '1';

      -- The upper memory-mode register controls whether the 32K pages are
      -- writable.
      upmem_we_inhibit_s <= upmem_mode_r(0);

      -- MegaCart bank select register.
      mc_bank_x         <= mc_bank_r;


      -- Memory access that is not a refresh cycle.
      if mreq_n_s = '0' and rfsh_n_s = '1' then

         -- Enable the external RAM when the SGM is enabled or
         -- the physical cartridge is NOT enabled.
         ext_ram_en_s <= sgm_en_r or (not real_cart_r);

         case cpu_addr_s(15 downto 13) is

         when "000" =>                 -- 8K  >0000 .. >1FFF   Loader, BIOS, or SGM RAM

            -- The loader is priority.
            if rom_loader_en_r = '1' then
               cpu_data_mux_s <= d_from_loader_s;

            -- The SGM must be enabled to be able to switch the BIOS for 8K RAM.
            elsif sgm_8k_en_r = '0' or sgm_en_r = '0' then
               cpu_data_mux_s <= d_from_bios_s;

            else
               ext_ram_we_n_s <= wr_n_s;
               cpu_data_mux_s <= d_from_ext512_s;
            end if;

         when "001" =>                 -- 8K >2000 .. >3FFF   Loader or SGM 24K RAM (if enabled)

            if rom_loader_en_r = '1' then
               cpu_data_mux_s <= d_from_loader_s;

            elsif sgm_en_r = '1' then
               ext_ram_we_n_s <= wr_n_s;
               cpu_data_mux_s <= d_from_ext512_s;
            end if;

         when "010" =>                 -- 8K >4000 .. >5FFF   Loader or SGM 24K RAM (if enabled)

            if rom_loader_en_r = '1' then
               --cpu_data_mux_s <= d_from_bios_s;
               cpu_data_mux_s <= d_from_loader_s;

            elsif sgm_en_r = '1' then
               ext_ram_we_n_s <= wr_n_s;
               cpu_data_mux_s <= d_from_ext512_s;
            end if;

         when "011" =>                 -- 8K  >6000 .. >7FFF   CV 1K and / or SGM 24K RAM

            if rom_loader_en_r = '1' or sgm_en_r = '1' then

               ext_ram_en_s <= '1';

               if cpu_addr_s(12 downto 10) = "000" then
                  -- Enable the CV 1K RAM when memory at >6000 is addressed
                  -- while the loader is active.  This allows the loader to
                  -- set up the CV 1K the same as the original BIOS would
                  -- prepare the RAM prior to jumping to a game.
                  cv_ram1k_en_s <= rom_loader_en_r;
               end if;

               -- A write-enable is required to keep the synthesizer from
               -- deciding that the loader ROM is redundant and removing parts
               -- of it from the design.  This is only enabled in the address
               -- range >6000 .. >7FFF when the loader module is internally
               -- preventing the write enable.
               rom_loader_we_n_s <= wr_n_s;

               -- Enable writing to the SGM, and read data from the SGM.
               ext_ram_we_n_s <= wr_n_s;
               cpu_data_mux_s <= d_from_ext512_s;

            else
               -- Normal CV 1K when the loader and SGM are disabled.
               cv_ram1k_en_s  <= '1';
               cpu_data_mux_s <= d_from_cv_ram1k_r;
            end if;

         when "100" =>                 -- 8K  >8000 .. >9FFF

            cart_en_80_n_s <= '0';

            if real_cart_r = '1' then
               cpu_data_mux_s <= cart_data_io;
            else
               cpu_data_mux_s <= d_from_ext512_s;
               ext_ram_we_n_s <= wr_n_s or upmem_we_inhibit_s;
            end if;

         when "101" =>                 -- 8K  >A000 .. >BFFF

            cart_en_A0_n_s <= '0';

            if real_cart_r = '1' then
               cpu_data_mux_s <= cart_data_io;
            else
               cpu_data_mux_s <= d_from_ext512_s;
               ext_ram_we_n_s <= wr_n_s or upmem_we_inhibit_s;
            end if;

         when "110" =>                 -- 8K  >C000 .. >DFFF

            cart_en_C0_n_s <= '0';

            if real_cart_r = '1' then
               cpu_data_mux_s <= cart_data_io;
            else
               cpu_data_mux_s <= d_from_ext512_s;
               ext_ram_we_n_s <= wr_n_s or upmem_we_inhibit_s;
           end if;

         when "111" =>                 -- 8K  >E000 .. >FFFF

            cart_en_E0_n_s <= '0';

            if real_cart_r = '1' then
               cpu_data_mux_s <= cart_data_io;
            else
               cpu_data_mux_s <= d_from_ext512_s;
               ext_ram_we_n_s <= wr_n_s or upmem_we_inhibit_s;
            end if;

            -- MegaCart bank register.  Since the Phoenix only has a 512K
            -- external memory, the bank register is only 5-bits instead of
            -- the original 6-bits.  Update on read or write.
            if cpu_addr_s(12 downto 6) = "1111111" then
               mc_bank_x <= cpu_addr_s(4 downto 0);
            end if;

         when others => null;
         end case;
      end if;
   end process;


   -- MegaCart memory size decode and address bank select formation.  The 512K
   -- external memory needs to be masked to *appear* to be whatever size EPROM
   -- image was loaded.  The mask is set by the loader and is used to ignore
   -- address bits that would not exist on EPROMs less than 512K.  The address
   -- is also created such that the top of the 512K is utilized since that is
   -- where the ROM data is loaded.
   megacart : process ( mc_mem_size_r, mc_bank_r )
   begin
      case ( mc_mem_size_r ) is
      when "00001" => mc_addr_x <= "1111" & mc_bank_r(0);            -- >01 = 32K
      when "00011" => mc_addr_x <= "111"  & mc_bank_r(1 downto 0);   -- >03 = 64K
      when "00111" => mc_addr_x <= "11"   & mc_bank_r(2 downto 0);   -- >07 = 128K
      when "01111" => mc_addr_x <= "1"    & mc_bank_r(3 downto 0);   -- >0F = 256K
      when others  => mc_addr_x <=          mc_bank_r;               -- >1F = 512K
      end case;
   end process;


   -- CV I/O Decode
   --
   -- A7 A6 A5 WR
   -- EN 4  2  1
   -- -----------
   -- 1  0  0  0  >80 .. >9F  (W) set controllers to keypad mode
   -- 1  0  0  1  >80 .. >9F  (R) not connected
   -- 1  0  1  0  >A0 .. >BF  (W) VDP csw_n, A0==mode
   -- 1  0  1  1  >A0 .. >BF  (R) VDP csr_n, A0==mode
   -- 1  1  0  0  >C0 .. >DF  (W) set controllers to joystick mode
   -- 1  1  0  1  >C0 .. >DF  (R) not connected
   -- 1  1  1  0  >E0 .. >FF  (W) sound chip (SN76489A)
   -- 1  1  1  1  >E0 .. >FF  (R) read controller data, A1=0 read controller 1, A1=1 read controller 2
   --

   io_decode_s <= cpu_addr_s(6 downto 5) & wr_n_s;

   process ( cpu_addr_s, io_decode_s, iorq_n_s, m1_n_s,
             d_from_ctrl_s, d_from_vdp_s )
   begin
      vdp_w_n_s         <= '1';
      vdp_r_n_s         <= '1';
      sn489_we_n_s      <= '1';
      ctrl_en_key_n_s   <= '1';
      ctrl_en_joy_n_s   <= '1';
      io_data_mux_s     <= d_from_ctrl_s;

      if iorq_n_s = '0' and m1_n_s = '1' and cpu_addr_s(7) = '1' then
         case io_decode_s is
         when "000" =>  ctrl_en_key_n_s   <= '0';
         when "010" =>  vdp_w_n_s         <= '0';
         when "011" =>  vdp_r_n_s         <= '0';
                        io_data_mux_s     <= d_from_vdp_s;
         when "100" =>  ctrl_en_joy_n_s   <= '0';
         when "110" =>  sn489_we_n_s      <= '0';
         when others => null;
         end case;
      end if;
   end process;


   -- Extended I/O Ports
   --
   -- Port  CPU Data
   -- --------------
   --  >50  WWWWWWWW (W) WSG / SGM-PSG Address Write
   --  >51  WWWWWWWW (W) WSG / SGM-PSG Data Write
   --  >52  RRRRRRRR (R) WSG / SGM-PSG Data Read
   --  >53  xxxxxx_W (W) 0 = SGM disable, 1 = SGM enable
   --       xxxxxxW_ (W) 0 = SGM Sound AY-8910, 1 = SGM Sound SEX-7264
   --
   --  >54  xxxxWWWW (W) 32K RAM bank select, 0000 is ignored
   --       xxxxRRRR (R) current bank
   --
   --  >55  xxxxxxxx (R) Loader disable, PoR re-enabled ROM loader
   --       xx__xxWW (W) Banking scheme
   --             00     None, physical cartridge, other bits ignored
   --             01     MegaCart: bank is address access >FFC0+bank
   --             10     SGM: Bank is data written to >FFFF
   --             11     Atari: >FF80, >FF90, >FFA0, >FFB0 ???
   --       xxWWxx__ (W) Select the upper memory operation:
   --         00         32K Upper Internal RAM, ignores bank scheme
   --         01         32K Expansion ROM, uses bank scheme
   --         10         32K Upper Expansion RAM, uses bank scheme
   --         11         32K Cartridge ROM, ignores bank scheme
   --
   --  >56  xxxxxx_W (W) SD-card CE_n (AKA SS_n), 0=enable
   --       xxxxxxW_ (W) SD-card speed, 1=400KHz, 0=12MHz
   --       RxxxxxRR (R) SD-card card-detect, bit >80 1=card inserted
   --
   --  >57  WWWWWWWW (W) SD-card Data Write
   --       RRRRRRRR (R) SD-card Data Read
   --
   --  >58  00001000 (R) Machine ID, Phoenix = 8
   --
   --  >59  xxxWWWWW (W) MegaCart memory size
   --          other     512K
   --          11111     512K, 32 16K-banks
   --          01111     256K, 16 16K-banks
   --          00111     128K,  8 16K-banks
   --          00011      64K,  4 16K-banks
   --          00001      32K,  2 16K-banks (same as original cart)
   --
   --  >7F  xxxxxxWx (W) 0 = 8K SGM RAM, 1 = 8K CV BIOS
   --
   process
   ( cpu_addr_s, iorq_n_s, m1_n_s, wr_n_s, rd_n_s
   , d_from_cpu_s, d_from_wsg_s, sgm_en_r, sgm_8k_en_r
   , ext_ram_bank_r, rom_loader_en_r
   , d_from_sd_s, sd_detect_r, sd_slow_clk_r, sd_spi_ss_n_r
   , real_cart_r, bank_mode_r, upmem_mode_r
   , mc_mem_size_r, mc_en_r
   ) begin
      sgm_en_x          <= sgm_en_r;
      sgm_8k_en_x       <= sgm_8k_en_r;

      ext_ram_bank_x    <= ext_ram_bank_r;

      rom_loader_en_x   <= rom_loader_en_r;

      wsg_bdir_s        <= '0';  -- bus direction
      wsg_bc_s          <= '0';  -- bus control

      -- SD-card
      sd_cs_n_s         <= '1';
      sd_slow_clk_x     <= sd_slow_clk_r;
      sd_spi_ss_n_x     <= sd_spi_ss_n_r;

      ex_data_mux_s     <= (others => '0');

      -- Upper memory and bank mode.
      real_cart_x       <= real_cart_r;   -- Will be '1' when bank_mode_r == "00"
      bank_mode_x       <= bank_mode_r;
      upmem_mode_x      <= upmem_mode_r;

      -- MegaCart support.
      mc_mem_size_x     <= mc_mem_size_r;
      mc_en_x           <= mc_en_r;


      if iorq_n_s = '0' and m1_n_s = '1' then
         case cpu_addr_s(7 downto 4) is

         when x"5" =>
            case cpu_addr_s(3 downto 0) is

            when x"0" =>                  -- port >50 PSG address write

               if wr_n_s = '0' then
                  wsg_bdir_s  <= '1';
                  wsg_bc_s    <= '1';
               end if;

            when x"1" =>                  -- port >51 PSG data write

               if wr_n_s = '0' then
                  wsg_bdir_s  <= '1';
               end if;

            when x"2" =>                  -- port >52 PSG data read

               if rd_n_s = '0' then
                  wsg_bc_s       <= '1';
                  ex_data_mux_s  <= d_from_wsg_s;
               end if;

            when x"3" =>                  -- port >53 SGM enable / disable

               if wr_n_s = '0' then
                  sgm_en_x       <= d_from_cpu_s(0);
               end if;

            when x"4" =>                  -- port >54 external RAM bank select

               if rd_n_s = '0' then
                  ex_data_mux_s  <= "0000" & ext_ram_bank_r;
               end if;

               -- Page 0 is reserved for the SGM and cannot be
               -- directly selected.
               if wr_n_s = '0' and d_from_cpu_s(3 downto 0) /= "0000" then
                  ext_ram_bank_x <= d_from_cpu_s(3 downto 0);
               end if;

            when x"5" =>                  -- port >55 ROM loader disable

               if rd_n_s = '0' then
                  rom_loader_en_x <= '0';
               end if;

               if wr_n_s = '0' then
                  -- A physical cartridge is selected when no banking is
                  -- selected.  In this case, all other upper memory banking
                  -- is ignored.
                  real_cart_x    <= d_from_cpu_s(1) nor d_from_cpu_s(0);
                  bank_mode_x    <= d_from_cpu_s(1 downto 0);
                  upmem_mode_x   <= d_from_cpu_s(5 downto 4);

                  -- MegaCart enable, true when MegaCart mode, and upper memory
                  -- is set to recognize banking.
                  mc_en_x        <= (not d_from_cpu_s(1)) and d_from_cpu_s(0) and
                                    (d_from_cpu_s(5) xor d_from_cpu_s(4));
               end if;

            when x"6" =>                  -- port >56 SD-card control

               if rd_n_s = '0' then
                  ex_data_mux_s  <= sd_detect_r & "00000" &
                                    sd_slow_clk_r & sd_spi_ss_n_r;
               end if;

               if wr_n_s = '0' then
                  sd_slow_clk_x <= d_from_cpu_s(1);
                  sd_spi_ss_n_x <= d_from_cpu_s(0);
               end if;

            when x"7" =>                  -- port >57 SD-card data read / write

               sd_cs_n_s <= '0';

               if rd_n_s = '0' then
                  ex_data_mux_s  <= d_from_sd_s;
               end if;

            when x"8" =>                  -- port >58 machine ID

               if rd_n_s = '0' then
                  ex_data_mux_s  <= machine_id_c;
               end if;

            when x"9" =>                  -- port >59 MegaCart memory size

               if wr_n_s = '0' then
                  mc_mem_size_x  <= d_from_cpu_s(4 downto 0);
               end if;

            when others => null;
            end case;
			when x"3" =>
				case cpu_addr_s(3 downto 0) is
					when x"1" =>                  -- port >30 AK keyboard interface check port
						--set port 30 to 30 so it will be different then port reads on 31,32,33 which will cause the system
						-- to not detect an Andy Keys keyboard
						if rd_n_s = '0' then
							ex_data_mux_s  <= cpu_addr_s(7 downto 0);
						end if;
				when others => null;
				end case;
			when others => null;
         end case;

         -- ADAM compatible selection for BIOS vs 8K RAM at >0000.
         if cpu_addr_s(7 downto 0) = x"7F" and wr_n_s = '0' then
            -- The SGM 8K enable, active high.
            sgm_8k_en_x <= not d_from_cpu_s(1);
         end if;
      end if;
   end process;


   -- System reset and register transfer.
   process (clk_25m0_i, reset_i, clk_3m58_en_i)
   begin
   if rising_edge(clk_25m0_i) then
      if reset_i = '1' then
         sgm_en_r          <= '0';           -- disable SGM on reset
         sgm_8k_en_r       <= '0';           -- BIOS over SGM 8K RAM at >0000
         ext_ram_bank_r    <= "0001";        -- external RAM bank select
         rom_loader_en_r   <= '1';           -- enabled ROM loader
         sd_detect_r       <= '0';           -- no card until sampled
         sd_slow_clk_r     <= '1';           -- fall-back to SD-card SPI slow clock
         sd_spi_ss_n_r     <= '1';           -- de-select the SD-card device
         real_cart_r       <= '1';           -- default to a real cartridge
         bank_mode_r       <= "00";          -- no banking
         upmem_mode_r      <= "00";          -- upper memory selection does not matter
         mc_mem_size_r     <= "01111";       -- default to 256K
         mc_bank_r         <= "11111";       -- default to top bank
         mc_addr_r         <= "11111";       -- default to top of 512K memory
         mc_en_r           <= '0';           -- disable MegaCart

      elsif clk_3m58_en_i = '1' then
         sgm_en_r          <= sgm_en_x;
         sgm_8k_en_r       <= sgm_8k_en_x;
         ext_ram_bank_r    <= ext_ram_bank_x;
         rom_loader_en_r   <= rom_loader_en_x;
         sd_detect_r       <= not sd_cd_n_i; -- sample and invert input directly
         sd_slow_clk_r     <= sd_slow_clk_x;
         sd_spi_ss_n_r     <= sd_spi_ss_n_x;
         real_cart_r       <= real_cart_x;
         bank_mode_r       <= bank_mode_x;
         upmem_mode_r      <= upmem_mode_x;
         mc_mem_size_r     <= mc_mem_size_x;
         mc_bank_r         <= mc_bank_x;
         mc_addr_r         <= mc_addr_x;
         mc_en_r           <= mc_en_x;

      end if;
   end if;
   end process;


-- EEPROM-based games use an alternative ROM page switch.
-- They are not MegaCarts (0xFFC0+page#) and not SGM multipage ROMs (page# written to 0xFFFF).
-- EEPROM games use 0xFF80/0xFF90/0xFFA0/0xFFB0 to switch 4x16kB pages.


end rtl;
