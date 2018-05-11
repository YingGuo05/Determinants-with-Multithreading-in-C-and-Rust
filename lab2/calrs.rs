/*
    ELEC 513 Lab 2
    Created by Ying Guo
    The fn det() uses online answers not created by myself.
*/
use std::thread;
use std::thread::sleep;
use std::time::{Duration,Instant};
use std::{mem,slice};
use std::fs::{File,OpenOptions};
use std::io::{Read, BufReader};
use std::io::{Write, BufWriter};
use std::io::prelude::*;

const BUFFER_SIZE : usize = 72000;
const NUM_THREADS : usize = 72;
const eps :f64= 0.0000001;
const N :usize = 32;

static mut TIMEOUT : i32 = 0;
static mut CALTIMES : i32 = 0;
static mut NTENSECONDS: i32 = 0;
static mut MARK : i32 = 0;
static mut calResult : [i64;BUFFER_SIZE] = [0i64;BUFFER_SIZE];
static mut inMatrix :[[i32;N*N];BUFFER_SIZE] = [[0i32;N*N];BUFFER_SIZE];
static mut buffer1 :[i8; N*N]= [0i8; N*N];
static mut buffer2:[i32; N*N] = [0i32; N*N];
static mut buffer :[u8; N*N*4] = [0u8; N*N*4];

//function for det()
fn zero(a: f64) -> bool
{  
    return (a > -eps) && (a < eps);  
}  

//calculate the matrix
fn det( C: [i32;N*N]) -> f64 
{   let mut mul= 0.0; 
    let mut result = 1.0;
    let mut a = [[0.0f64; N]; N];//DOUBLE 
    let mut b = [0;N]; 
    for i in 0..N {
    	for j in 0..N {
            a[i][j] = C[i*N+j] as f64;
        }
    }
    for i in 0..N {
        b[i] = i as i32;
    } 
    for i in 0..N {  
        if zero(a[b[i]as usize][i])==true {
            for j in (i+1)..N{
                if zero(a[b[j] as usize][i])==false
                {	
                    
                    let temp = b[i];
                    b[i] = b[j];
                    b[j] = temp; 
                    result = -result;
                    break;  
                } 
            } 
        }  
        result = result * a[b[i] as usize][i];
        for j in (i+1)..N {
            mul = a[b[j] as usize][i]/a[b[i] as usize][i];
            for k in i..N {
                a[b[j] as usize][k] = a[b[j] as usize][k] - a[b[i] as usize][k]*mul;
            }       
        }      
    }
    return result;  
}


fn main(){
    let mut bufferVec = vec![0i32;BUFFER_SIZE];
    //reset the dummy
    println!("Reset the device.....");
    let mut reset = File::create("/sys/module/dummy/parameters/no_of_reads").expect("Fail to open!");
    reset.write_all(b"0");
    drop(reset);
    println!("**********Finish reset!************");
    println!("*************Testing... *************");
    let mut file = OpenOptions::new()
                    .read(true)
                    .write(true)
                    .create(true)
                    .open("/dev/dummychar")
                    .unwrap();
    let mut nth = 1;
    for i in 0..18 {
        let threadruntime = Duration::from_secs(10);
        let begin = Instant::now();
        let mut threadRunning = true;
        while threadRunning{
            let ela = begin.elapsed();
            threadRunning = ela <= threadruntime;
            for k in 0..BUFFER_SIZE{
                unsafe{file.read(&mut buffer[..]).expect("Fail to read");};
                for i in 0..4*1024{
                    if i % 4 == 0{
                        unsafe{ buffer1[i/4] = buffer[i] as i8;};
                        unsafe{ buffer2[i/4] = buffer1[i/4] as i32;};
                    }
                }
                for j in 0..1024{
                    unsafe{inMatrix[k][j] = buffer2[j];};
                }
            }
            let res = (BUFFER_SIZE/NUM_THREADS) as i32;
            let mut calThreads = vec![];
            for i in 0..NUM_THREADS{
                bufferVec[i] = i as i32;
                let lowbounds = bufferVec[i]*res;
                let upperbounds = (bufferVec[i]+1)*res;
                calThreads.push((thread::spawn(move || {
                    for j in lowbounds as usize..upperbounds as usize{
                        unsafe{
                            CALTIMES = CALTIMES+1;
                        }
                        unsafe{
                            calResult[j as usize] = det(inMatrix[j as usize]) as i64;
                        }
                    }
                })));
            }
            for nthreads in calThreads{
                let _ = nthreads.join().unwrap();
            }
            for k in 0..BUFFER_SIZE{
                let bts: [u8;8] = unsafe{
                    std::mem::transmute::<i64,[u8;8]>(calResult[k])
                };
                file.write(&bts).expect("fail to write");
                file.flush().expect("The calculate answer is not correct");
            }
        }
        unsafe{
            println!("in {}th 10 seconds, the program calculates {} times",nth,CALTIMES);
            CALTIMES = 0;
        }
        nth = nth+1;
    }
    println!("**********TESTING ENDS!**********"); 
}
