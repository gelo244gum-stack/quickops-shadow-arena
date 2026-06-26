import assert from 'node:assert/strict';
import test from 'node:test';

import {
  formatPercent,
  formatPrice,
  formatQuantity,
  formatVolume,
} from './formatters.js';

test('formatPrice returns the fallback dash for non-finite values', () => {
  assert.equal(formatPrice(Infinity), ' - ');
  assert.equal(formatPrice(-Infinity), ' - ');
  assert.equal(formatPrice(NaN), ' - ');
});

test('formatPrice selects decimals from the documented magnitude thresholds', () => {
  assert.equal(formatPrice(12345.6789), '12345.68');
  assert.equal(formatPrice(123.456789), '123.4568');
  assert.equal(formatPrice(1.234567), '1.2346');
  assert.equal(formatPrice(0.01234567), '0.012346');
  assert.equal(formatPrice(0.0000123456), '0.0000123456');
});

test('formatQuantity returns zero directly and applies K/M suffixes', () => {
  assert.equal(formatQuantity(0), '0');
  assert.equal(formatQuantity(12_345), '12.3K');
  assert.equal(formatQuantity(1_250_000), '1.25M');
});

test('formatVolume applies B/M/K suffixes and falls back for zero', () => {
  assert.equal(formatVolume(2_500_000_000), '2.50B');
  assert.equal(formatVolume(3_400_000), '3.40M');
  assert.equal(formatVolume(1_200), '1.2K');
  assert.equal(formatVolume(999), '999');
  assert.equal(formatVolume(0), ' - ');
});

test('formatPercent includes signs for positive, negative, and zero values', () => {
  assert.equal(formatPercent(12.345), '+12.35%');
  assert.equal(formatPercent(-0.5), '-0.50%');
  assert.equal(formatPercent(0), '+0.00%');
  assert.equal(formatPercent(Infinity), ' - ');
});
