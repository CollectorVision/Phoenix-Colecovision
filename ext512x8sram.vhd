--
-- Phoenix External IS61LV5128AL 10ns SRAM Interface
--

-- Released under the 3-Clause BSD License:
--
-- Copyright 2011-2018 Matthew Hagerty (matthew <at> dnotq <dot> io)
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


library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ext512x8sram is
   port (
      clk_i          :  in    std_logic;  --  21.47727 (21.739 PLL) MHz
      -- Z80 CPU
      cpu_addr_i     :  in    std_logic_vector(18 downto 0);
      cpu_en_i       :  in    std_logic;
      cpu_we_n_i     :  in    std_logic;
      cpu_data_i     :  in    std_logic_vector(7 downto 0);
      cpu_data_o     :  out   std_logic_vector(7 downto 0);

      -- External SRAM Interface
      sram_addr_o    :  out   std_logic_vector(18 downto 0);
      sram_data_io   :  inout std_logic_vector(7 downto 0);
      sram_ce_n_o    :  out   std_logic := '1';
      sram_oe_n_o    :  out   std_logic := '1';
      sram_we_n_o    :  out   std_logic := '1'
   );
end entity;

architecture rtl of ext512x8sram is

   signal addr_r, addr_x               : std_logic_vector(18 downto 0);
   signal d_to_ext_r, d_to_ext_x       : std_logic_vector( 7 downto 0);
   signal d_from_ext_r, d_from_ext_x   : std_logic_vector( 7 downto 0);
   signal ce_n_r, ce_n_x               : std_logic := '1';
   signal we_n_r, we_n_x               : std_logic := '1';

   type ramfsm_t is (st_idle, st_rdwr);
   signal ram_st_r, ram_st_x           : ramfsm_t := st_idle;

begin

   -- The 21MHz clock is fast enough to multiplex access to the SRAM if
   -- necessary and is not even half of the max speed of the SRAM.
   
   -- The clock used should be synchronous to the input enable signal
   -- otherwise clock domain synchronization should be performed.
   
   -- SEt the external RAM outputs to be read/write controlled.
   sram_oe_n_o <= '0';

   -- External RAM data tri-state I/O.
   sram_data_io <= d_to_ext_r when we_n_r = '0' else (others => 'Z');

   -- All interfacing to the external RAM is registered for stability.
   sram_addr_o <= addr_r;
   sram_ce_n_o <= ce_n_r;
   sram_we_n_o <= we_n_r;
   
   cpu_data_o  <= d_from_ext_r;


   process ( ram_st_r, cpu_en_i, cpu_addr_i, cpu_we_n_i, cpu_data_i,
             d_from_ext_r, sram_data_io )
   begin
      
      ram_st_x       <= ram_st_r;
      addr_x         <= cpu_addr_i;
      d_to_ext_x     <= cpu_data_i;
      d_from_ext_x   <= d_from_ext_r;
      ce_n_x         <= '1';
      we_n_x         <= '1';

      case ram_st_r is

      when st_idle =>

         -- The cpu_en_i must be synchronous to the clock.
         if cpu_en_i = '1' then
            ram_st_x <= st_rdwr;
            ce_n_x   <= '0';
         end if;

      when st_rdwr =>

         if cpu_en_i = '0' then
            ram_st_x <= st_idle;

         else
            -- As long as the memory request is active, the write enable
            -- defines a read or write operation.
            ce_n_x   <= '0';
            we_n_x   <= cpu_we_n_i;

            if cpu_we_n_i = '1' then
               -- Register input data during a read cycle.
               d_from_ext_x <= sram_data_io;
            end if;

         end if;

      end case;
   end process;

   process ( clk_i )
   begin
      if rising_edge(clk_i) then
         ram_st_r       <= ram_st_x;
         addr_r         <= addr_x;
         d_to_ext_r     <= d_to_ext_x;
         d_from_ext_r   <= d_from_ext_x;
         ce_n_r         <= ce_n_x;
         we_n_r         <= we_n_x;
      end if;
   end process;

end rtl;
