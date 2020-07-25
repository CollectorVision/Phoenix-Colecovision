--
-- Internal ROM Loader
--
-- Set up as a RAM to force the synthesizer to keep the memory
-- it would normally consider redundant (and thus remove).
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;

entity romloader is
   port (
      clock_i        : in  std_logic;
      we_n_i         : in  std_logic;
      addr_i         : in  std_logic_vector(14 downto 0);
      data_i         : in  std_logic_vector( 7 downto 0);
      data_o         : out std_logic_vector( 7 downto 0)
   );
end;

architecture rtl of romloader is

   type ram_t is array(0 to 2047) of std_logic_vector(7 downto 0);
   signal ram_1 : ram_t := (

      -- Set up the following program in RAM at >6000:
      --
      -- DB 55     IN   A,(0x55)
      -- C3 00 00  JP   0x0000
      --
      -- The loader program:
      x"21",x"0E",x"00", -- 0000   21 0E 00               LD   HL,CODE
      x"11",x"00",x"60", -- 0003   11 00 60               LD   DE,0x6000
      x"01",x"05",x"00", -- 0006   01 05 00               LD   BC,ENDCODE-CODE
      x"ED",x"B0",       -- 0009   ED B0                  LDIR
      x"C3",x"00",x"60", -- 000B   C3 00 60               JP   0x6000
                         -- 000E                CODE:
      x"DB",x"55",       -- 000E   DB 55                  IN   A,(0x55)
      x"C3",x"00",x"00", -- 0010   C3 00 00               JP   0x0000
                         -- 0013                ENDCODE:
      x"00",             -- 0013   00                     DB   0

      others => (others => '0')
   );

   signal ram_2 : ram_t := (others => (others => '0'));
   signal ram_3 : ram_t := (others => (others => '0'));
   signal ram_4 : ram_t := (others => (others => '0'));
   signal ram_5 : ram_t := (others => (others => '0'));
   signal ram_6 : ram_t := (others => (others => '0'));
   signal ram_7 : ram_t := (others => (others => '0'));
   signal ram_8 : ram_t := (others => (others => '0'));
   signal ram_9 : ram_t := (others => (others => '0'));
   signal ram_A : ram_t := (others => (others => '0'));
   signal ram_B : ram_t := (others => (others => '0'));
   signal ram_C : ram_t := (others => (others => '0'));

   signal we1_s, we2_s, we3_s    : std_logic := '1';
   signal we4_s, we5_s, we6_s    : std_logic := '1';
   signal we7_s, we8_s, we9_s    : std_logic := '1';
   signal weA_s, weB_s, weC_s    : std_logic := '1';
   signal d1_r, d2_r, d3_r       : std_logic_vector( 7 downto 0) := (others => '0');
   signal d4_r, d5_r, d6_r       : std_logic_vector( 7 downto 0) := (others => '0');
   signal d7_r, d8_r, d9_r       : std_logic_vector( 7 downto 0) := (others => '0');
   signal dA_r, dB_r, dC_r       : std_logic_vector( 7 downto 0) := (others => '0');

   signal bank_s                 : std_logic_vector( 3 downto 0) := "0000";
   signal addr2k_s               : std_logic_vector(10 downto 0) := (others => '0');

begin

   -- Twelve banks of 2K.
   bank_s   <= addr_i(14 downto 11);
   addr2k_s <= addr_i(10 downto  0);

   -- Write enable only when addressing the 24K.
   we1_s <= we_n_i when bank_s = "0000" else '1';
   we2_s <= we_n_i when bank_s = "0001" else '1';
   we3_s <= we_n_i when bank_s = "0010" else '1';
   we4_s <= we_n_i when bank_s = "0011" else '1';
   we5_s <= we_n_i when bank_s = "0100" else '1';
   we6_s <= we_n_i when bank_s = "0101" else '1';
   we7_s <= we_n_i when bank_s = "0110" else '1';
   we8_s <= we_n_i when bank_s = "0111" else '1';
   we9_s <= we_n_i when bank_s = "1000" else '1';
   weA_s <= we_n_i when bank_s = "1001" else '1';
   weB_s <= we_n_i when bank_s = "1010" else '1';
   weC_s <= we_n_i when bank_s = "1011" else '1';

   process (clock_i)
   begin
   if rising_edge(clock_i) then

      if we1_s = '0' then ram_1(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we2_s = '0' then ram_2(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we3_s = '0' then ram_3(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we4_s = '0' then ram_4(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we5_s = '0' then ram_5(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we6_s = '0' then ram_6(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we7_s = '0' then ram_7(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we8_s = '0' then ram_8(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if we9_s = '0' then ram_9(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if weA_s = '0' then ram_A(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if weB_s = '0' then ram_B(to_integer(unsigned(addr2k_s))) <= data_i; end if;
      if weC_s = '0' then ram_C(to_integer(unsigned(addr2k_s))) <= data_i; end if;

      d1_r <= ram_1(to_integer(unsigned(addr2k_s)));
      d2_r <= ram_2(to_integer(unsigned(addr2k_s)));
      d3_r <= ram_3(to_integer(unsigned(addr2k_s)));
      d4_r <= ram_4(to_integer(unsigned(addr2k_s)));
      d5_r <= ram_5(to_integer(unsigned(addr2k_s)));
      d6_r <= ram_6(to_integer(unsigned(addr2k_s)));
      d7_r <= ram_7(to_integer(unsigned(addr2k_s)));
      d8_r <= ram_8(to_integer(unsigned(addr2k_s)));
      d9_r <= ram_9(to_integer(unsigned(addr2k_s)));
      dA_r <= ram_A(to_integer(unsigned(addr2k_s)));
      dB_r <= ram_B(to_integer(unsigned(addr2k_s)));
      dC_r <= ram_C(to_integer(unsigned(addr2k_s)));

   end if;
   end process;

   -- Output selector.
   process ( bank_s, d1_r, d2_r, d3_r, d4_r, d5_r, d6_r,
                     d7_r, d8_r, d9_r, dA_r, dB_r, dC_r)
   begin
      case bank_s is
      when "0000" =>    data_o <= d1_r;
      when "0001" =>    data_o <= d2_r;
      when "0010" =>    data_o <= d3_r;
      when "0011" =>    data_o <= d4_r;
      when "0100" =>    data_o <= d5_r;
      when "0101" =>    data_o <= d6_r;
      when "0110" =>    data_o <= d7_r;
      when "0111" =>    data_o <= d8_r;
      when "1000" =>    data_o <= d9_r;
      when "1001" =>    data_o <= dA_r;
      when "1010" =>    data_o <= dB_r;
      when others =>    data_o <= dC_r;
      end case;
   end process;

end architecture;
