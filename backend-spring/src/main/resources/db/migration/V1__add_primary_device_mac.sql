-- Add primary_device_mac column to elderly_members table
ALTER TABLE elderly_members ADD COLUMN IF NOT EXISTS primary_device_mac VARCHAR(50);
