/* vertaccel -- Compute vertical acceleration from IMU 
 *
 * Copyright 2016-2019 Baptiste PELLEGRIN
 * 
 * This file is part of GNUVario.
 *
 * GNUVario is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNUVario is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <vertaccel2.h>

#include <Arduino.h>
//#include <LightInvensense.h>

#ifdef VERTACCEL_ENABLE_EEPROM
#include <eepromHAL.h>
#endif

/* static variables */
#ifdef VERTACCEL_STATIC_CALIBRATION
constexpr uint8_t Vertaccel::gyroCalArray[12];
constexpr int32_t Vertaccel::accelCalArray[3];
#endif


#ifdef VERTACCEL_ENABLE_EEPROM
/******************/
/* EEPROM methods */
/******************/
void readEEPROMValues(int address, uint16_t eepromTag, int length, uint8_t* data) {

  /* read tag */
  uint16_t tag = EEPROMHAL.read(address);
  address++;
  tag = (tag<<8) + EEPROMHAL.read(address);
  address++;

  /* read values */
  for( uint8_t i = 0; i<length; i++) {
    if( tag == eepromTag ) {
      *data = EEPROMHAL.read(address);
    } else {
      *data = 0;
    }
    data++;
    address++;
  }
}


void writeEEPROMValues(int address, uint16_t eepromTag, int length, const uint8_t* data) {

  /* write tag */
  EEPROMHAL.write(address, (eepromTag>>8) & 0xff);
  EEPROMHAL.write(address + 0x01, eepromTag & 0xff);
  address += 2;
  
  /* write values */
  for( uint8_t i = 0; i<length; i++) {
    EEPROMHAL.write(address, *data);
    data++;
    address++;
  }
}


VertaccelSettings Vertaccel::readEEPROMSettings(void) {

  VertaccelSettings eepromSettings;
  readGyroCalibration(eepromSettings.gyroCal);
  readAccelCalibration(eepromSettings.accelCal);
#ifdef AK89xx_SECONDARY 
  readMagCalibration(eepromSettings.magCal);
#endif //AK89xx_SECONDARY

  return eepromSettings;
}


void Vertaccel::saveGyroCalibration(const uint8_t* gyroCal) {

  writeEEPROMValues(VERTACCEL_GYRO_CAL_EEPROM_ADDR, VERTACCEL_GYRO_CAL_EEPROM_TAG, 12, gyroCal);
}

void Vertaccel::readGyroCalibration(uint8_t* gyroCal) {

  readEEPROMValues(VERTACCEL_GYRO_CAL_EEPROM_ADDR, VERTACCEL_GYRO_CAL_EEPROM_TAG, 12, gyroCal);
}

void Vertaccel::saveAccelCalibration(const VertaccelCalibration& accelCal) {

  writeEEPROMValues(VERTACCEL_ACCEL_CAL_EEPROM_ADDR, VERTACCEL_ACCEL_CAL_EEPROM_TAG, sizeof(VertaccelCalibration), (uint8_t*)(&accelCal));
}

void Vertaccel::readAccelCalibration(VertaccelCalibration& accelCal) {

  readEEPROMValues(VERTACCEL_ACCEL_CAL_EEPROM_ADDR, VERTACCEL_ACCEL_CAL_EEPROM_TAG, sizeof(VertaccelCalibration), (uint8_t*)(&accelCal));
}

#ifdef AK89xx_SECONDARY
void Vertaccel::saveMagCalibration(const VertaccelCalibration& magCal) {

  writeEEPROMValues(VERTACCEL_MAG_CAL_EEPROM_ADDR, VERTACCEL_MAG_CAL_EEPROM_TAG, sizeof(VertaccelCalibration), (uint8_t*)(&magCal));
}

void Vertaccel::readMagCalibration(VertaccelCalibration& magCal) {

  readEEPROMValues(VERTACCEL_MAG_CAL_EEPROM_ADDR, VERTACCEL_MAG_CAL_EEPROM_TAG, sizeof(VertaccelCalibration), (uint8_t*)(&magCal));
}
#endif //AK89xx_SECONDARY
#endif //VERTACCEL_ENABLE_EEPROM
  


/***************/
/* init device */
/***************/
void Vertaccel::init(void) {

  /* init MPU */
//  fastMPUInit(false);

#ifndef VERTACCEL_STATIC_CALIBRATION
  /* read calibration coeff from EEPROM if needed */
  settings = readEEPROMSettings();

  /* set gyro calibration in the DMP */
//  fastMPUSetGyroBias(settings.gyroCal);
#else
  /* set gyro calibration in the DMP */
//  fastMPUSetGyroBias(gyroCalArray);
#endif

  

#ifndef VERTACCEL_STATIC_CALIBRATION
  /* set accel calibration in the DMP */
  int32_t accelBias[3];
  for(int i = 0; i<3; i++) 
    accelBias[i] = (int32_t)settings.accelCal.bias[i] << (15 - VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER);

//  fastMPUSetAccelBiasQ15(accelBias);
#else
  /* set accel calibration in the DMP */
//  fastMPUSetAccelBiasQ15(accelCalArray);
#endif
    
  /* start DMP */
//  fastMPUStart();
}


/* compute vertical vector and vertical accel from IMU data */
void Vertaccel::compute(int16_t *imuAccel, int32_t *imuQuat, double* vertVector, double& vertAccel) {

  /*---------------------------------------*/
  /*   vertical acceleration computation   */
  /*---------------------------------------*/

  /***************************/
  /* normalize and calibrate */
  /***************************/
  double accel[3]; 
  double quat[4];
  
#ifndef VERTACCEL_STATIC_CALIBRATION
  for(int i = 0; i<3; i++) {
    int64_t calibratedAccel = (int64_t)imuAccel[i] << VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER;
    calibratedAccel -= (int64_t)settings.accelCal.bias[i];
    calibratedAccel *= ((int64_t)settings.accelCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
    accel[i] = ((double)calibratedAccel)/((double)((int64_t)1 << (VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_ACCEL_SCALE_SHIFT)));
  }
#else
  /* inline for optimization */
  int64_t calibratedAccel;
  calibratedAccel = (int64_t)imuAccel[0] << VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER;
  calibratedAccel -= (int64_t)settings.accelCal.bias[0];
  calibratedAccel *= ((int64_t)settings.accelCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
  accel[0] = ((double)calibratedAccel)/((double)((int64_t)1 << (VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_ACCEL_SCALE_SHIFT)));

  calibratedAccel = (int64_t)imuAccel[1] << VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER;
  calibratedAccel -= (int64_t)settings.accelCal.bias[1];
  calibratedAccel *= ((int64_t)settings.accelCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
  accel[1] = ((double)calibratedAccel)/((double)((int64_t)1 << (VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_ACCEL_SCALE_SHIFT)));

  calibratedAccel = (int64_t)imuAccel[2] << VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER;
  calibratedAccel -= (int64_t)settings.accelCal.bias[2];
  calibratedAccel *= ((int64_t)settings.accelCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
  accel[2] = ((double)calibratedAccel)/((double)((int64_t)1 << (VERTACCEL_ACCEL_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_ACCEL_SCALE_SHIFT)));
#endif
  
  for(int i = 0; i<4; i++)
    quat[i] = ((double)imuQuat[i])/LIGHT_INVENSENSE_QUAT_SCALE;
  
  
  /******************************/
  /* real and vert acceleration */
  /******************************/
  
  /* compute vertical direction from quaternions */
  vertVector[0] = 2*(quat[1]*quat[3]-quat[0]*quat[2]);
  vertVector[1] = 2*(quat[2]*quat[3]+quat[0]*quat[1]);
  vertVector[2] = 2*(quat[0]*quat[0]+quat[3]*quat[3])-1;
  
  /* compute real acceleration (without gravity) */
  double ra[3];
  for(int i = 0; i<3; i++) 
    ra[i] = accel[i] - vertVector[i];
  
  /* compute vertical acceleration */
  vertAccel = (vertVector[0]*ra[0] + vertVector[1]*ra[1] + vertVector[2]*ra[2]) * VERTACCEL_G_TO_MS;
}


#ifdef AK89xx_SECONDARY
void Vertaccel::computeNorthVector(double* vertVector, int16_t* mag, double* northVector) {

  /*-------------------------------*/
  /*   north vector computation    */
  /*-------------------------------*/
  
  double n[3];

#ifndef VERTACCEL_STATIC_CALIBRATION
  for(int i = 0; i<3; i++) {
    int64_t calibratedMag = ((int64_t)mag[i]) << VERTACCEL_MAG_CAL_BIAS_MULTIPLIER;
    calibratedMag -= (int64_t)settings.magCal.bias[i];
    calibratedMag *= ((int64_t)settings.magCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
    n[i] = ((double)calibratedMag)/((double)((int64_t)1 << (VERTACCEL_MAG_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_MAG_PROJ_SCALE_SHIFT)));
  }
#else
  /* inline for optimization */
  int64_t calibratedMag;
  calibratedMag = ((int64_t)mag[0]) << VERTACCEL_MAG_CAL_BIAS_MULTIPLIER;
  calibratedMag -= (int64_t)settings.magCal.bias[0];
  calibratedMag *= ((int64_t)settings.magCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
  n[0] = ((double)calibratedMag)/((double)((int64_t)1 << (VERTACCEL_MAG_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_MAG_PROJ_SCALE_SHIFT)));

  calibratedMag = ((int64_t)mag[1]) << VERTACCEL_MAG_CAL_BIAS_MULTIPLIER;
  calibratedMag -= (int64_t)settings.magCal.bias[1];
  calibratedMag *= ((int64_t)settings.magCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
  n[1] = ((double)calibratedMag)/((double)((int64_t)1 << (VERTACCEL_MAG_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_MAG_PROJ_SCALE_SHIFT)));

  calibratedMag = ((int64_t)mag[2]) << VERTACCEL_MAG_CAL_BIAS_MULTIPLIER;
  calibratedMag -= (int64_t)settings.magCal.bias[2];
  calibratedMag *= ((int64_t)settings.magCal.scale + ((int64_t)1 << VERTACCEL_CAL_SCALE_MULTIPLIER));
  n[2] = ((double)calibratedMag)/((double)((int64_t)1 << (VERTACCEL_MAG_CAL_BIAS_MULTIPLIER + VERTACCEL_CAL_SCALE_MULTIPLIER + LIGHT_INVENSENSE_MAG_PROJ_SCALE_SHIFT)));
#endif
        
  /* compute north vector by applying rotation from v to z to vector n */
  vertVector[2] = -1.0 - vertVector[2];
  northVector[0] = (1+vertVector[0]*vertVector[0]/vertVector[2])*n[0] + (vertVector[0]*vertVector[1]/vertVector[2])*n[1] - vertVector[0]*n[2];
  northVector[1] = (vertVector[0]*vertVector[1]/vertVector[2])*n[0] + (1+vertVector[1]*vertVector[1]/vertVector[2])*n[1] - vertVector[1]*n[2];
}

#endif //AK89xx_SECONDARY
