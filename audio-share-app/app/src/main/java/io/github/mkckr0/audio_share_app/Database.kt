package io.github.mkckr0.audio_share_app

import androidx.room.*

@Entity
data class Settings (
    @PrimaryKey val key: String,
    val value: String
)

@Dao
interface SettingsDao {

    @Query("SELECT value FROM settings WHERE `key` = :key")
    fun getValue(key: String) : String

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun setValue(settings: Settings)
}

@Database(entities = [Settings::class], version = 1, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun settingsDao(): SettingsDao
}