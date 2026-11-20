// Test responses for module selection only; no physical movement is modeled.
using Antmicro.Renode.Peripherals.I2C;

namespace Antmicro.Renode.Peripherals.Mocks
{
    // Respond to the product-number, reset and ATI checks during boot.
    public class AnantaIQS : II2CPeripheral
    {
        private int address;
        public void Reset() { address = 0; }
        public void FinishTransmission() {}
        public void Write(byte[] data)
        {
            if (data.Length >= 2) address = data[0] | (data[1] << 8);
        }
        public byte[] Read(int count)
        {
            var value = new byte[count];
            for (int i = 0; i < count; i++, address++)
                value[i] = (byte)(address == 0x1000 ? 0xbc : address == 0x1001 ? 9 : address == 0x1020 ? 0x80 : 0);
            return value;
        }
    }

    public class AnantaPinnacle : II2CPeripheral
    {
        private byte[] registers = new byte[32];
        private int address;
        public AnantaPinnacle() { Reset(); }
        public void Reset() { System.Array.Clear(registers, 0, registers.Length); registers[0] = 7; }
        public void FinishTransmission() {}
        public byte[] Read(int count)
        {
            var result = new byte[count];
            for (int i = 0; i < count; i++) result[i] = registers[(address + i) & 31];
            return result;
        }
        public void Write(byte[] data)
        {
            for (int i = 0; i < data.Length; i += 2)
            {
                address = data[i] & 31;
                if (i + 1 == data.Length) break;
                registers[address] = data[i + 1];
                if (address == 3 && (data[i + 1] & 1) != 0) registers[2] |= 8;
                if (address == 30) registers[30] = 0;
            }
        }
    }
}
