import React from 'react'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import ElderlyLogin from './components/ElderlyLogin'
import ElderlyAuth from './components/ElderlyAuth'
import ElderlyDashboard from './components/ElderlyDashboard'
import ElderlyHome from './components/ElderlyHome'
import FallAlert from './components/FallAlert'

function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/login" element={<ElderlyLogin />} />
        <Route path="/register" element={<ElderlyAuth />} />
        <Route path="/dashboard" element={<ElderlyDashboard />} />
        <Route path="/home" element={<ElderlyHome />} />
        <Route path="/fall-alert" element={<FallAlert />} />
        <Route path="/" element={<ElderlyLogin />} />
      </Routes>
    </BrowserRouter>
  )
}

export default App
