--
-- Phoenix ColecoVision
--
-- Matthew Hagerty
--
-- Generates the registered 3.58MHz and 1.79MHz clock enable strobes.
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity clock_enables is
port
   ( clk_21m48_i           : in     std_logic
   ; clk_3m58_en_r_o       : out    std_logic -- 3.58MHz clock enable strobe.
   ; clk_1m79_en_r_o       : out    std_logic -- 1.79MHz clock enable strobe.
   );
end entity;

architecture rtl of clock_enables is

   signal cnt_r            : unsigned(3 downto 0) := x"0";
   signal cnt_x            : unsigned(3 downto 0);

   signal err_r            : unsigned(6 downto 0) := (others => '0');
   signal err_x            : unsigned(6 downto 0);

   signal en_cnt_s         : std_logic;
   signal en_3m58_s        : std_logic;

   signal en_3m58_r        : std_logic := '0';
   signal en_3m58_x        : std_logic;
   signal en_1m79_r        : std_logic := '0';
   signal en_1m79_x        : std_logic;

begin

   -- Count 0 to 11 to provide a divide-by-6 and divide-by-12.
   cnt_x <= x"0" when cnt_r = 11 else cnt_r + 1;

   en_cnt_s  <= '1' when err_r < 82 else '0';
   en_3m58_s <= '1' when cnt_r = 0 or cnt_r = 6 else '0';

   -- The PLL clock is actually 21,739,130Hz, instead of 21,477,270Hz.  That
   -- creates an error in the 3,579,545Hz clock of:
   -- 3,623,188 - 3,579,545 = 43,643
   --
   -- 3,623,188 / 43,643 = 83
   --
   -- Skipping an enable every 83 counts will accommodate for the 43,643 error.

   err_x <= (others => '0') when err_r = 82 else err_r + 1;

   en_3m58_x <= en_cnt_s and en_3m58_s;
   en_1m79_x <= en_3m58_x when cnt_r = 0 else '0';

   process ( clk_21m48_i )
   begin
      if rising_edge(clk_21m48_i) then
         cnt_r     <= cnt_x;
         en_3m58_r <= en_3m58_x;
         en_1m79_r <= en_1m79_x;

         if en_3m58_s = '1' then
            err_r <= err_x;
         end if;

      end if;
   end process;

   clk_3m58_en_r_o <= en_3m58_r;
   clk_1m79_en_r_o <= en_1m79_r;

end rtl;
